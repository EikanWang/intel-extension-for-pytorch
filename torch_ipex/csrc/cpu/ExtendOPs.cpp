#include "ExtendOPs.h"
#include "../utils.h"
#include "CustomOPs.h"
#include "DevOPs.h"
#include "FusionOPs.h"
#include "aten/aten.hpp"
#include "bf16/vec/bf16_vec_kernel.h"
#include "dil/dil.hpp"
#include "xsmm/libxsmm_utils.h"
#include <ATen/Parallel.h>
#include <ATen/MatrixRef.h>
#include <algorithm>
#include <c10/util/Exception.h>
#include <torch/csrc/autograd/function.h>

namespace torch_ipex {

void AtenIpexTypeExt::packed_add_(at::Tensor &top_half, at::Tensor &bot_half,
                                  const at::Tensor &grad, float alpha) {
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(grad.scalar_type() ==
                                   at::ScalarType::BFloat16);
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(top_half.scalar_type() ==
                                   at::ScalarType::BFloat16);
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(bot_half.scalar_type() ==
                                   at::ScalarType::BFloat16);
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(grad.device().type() ==
                                   at::DeviceType::XPU);
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(top_half.device().type() ==
                                   at::DeviceType::XPU);
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(bot_half.device().type() ==
                                   at::DeviceType::XPU);
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(top_half.sizes() == bot_half.sizes());
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(top_half.is_contiguous());
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(bot_half.is_contiguous());

#if defined(IPEX_PROFILE_OP)
  RECORD_FUNCTION("packed_add_", std::vector<c10::IValue>({top_half, bot_half, grad, alpha}));
#endif

  if (grad.is_sparse()) {
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(top_half.dim() == 2);
    auto sparse_nnz = grad._nnz();
    auto sparse_dim = grad.sparse_dim();
    auto values = grad._values();
    auto indices = grad._indices();
    auto entry_range = top_half.size(0);
    auto feature_size = values.stride(0);
    auto indices_accessor = indices.accessor<int64_t, 2>();

    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(values.is_contiguous());
    auto value_ptr = values.data_ptr<at::BFloat16>();
    auto top_half_ptr = top_half.data_ptr<at::BFloat16>();
    auto bot_half_ptr = bot_half.data_ptr<at::BFloat16>();

    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(value_ptr != nullptr);
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(top_half_ptr != nullptr);
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(bot_half_ptr != nullptr);

    std::vector<int64_t> sparse_stride(sparse_dim);
    for (int64_t d = 0; d < sparse_dim; d++) {
      sparse_stride[d] = top_half.stride(d);
    }

    int32_t max_threads = at::get_num_threads();
    max_threads = (entry_range < max_threads) ? entry_range : max_threads;
    int64_t avg_size = entry_range / max_threads;
    int64_t tail_size = entry_range % max_threads;
    std::vector<int64_t> chunk_size(max_threads, avg_size);
    std::transform(chunk_size.begin(), chunk_size.begin() + tail_size,
                   chunk_size.begin(),
                   [](int64_t a) -> int64_t { return a + 1; });
    std::vector<int64_t> acc_chunk_size(max_threads + 1);
    for (int64_t i = 1; i < max_threads + 1; i++) {
      acc_chunk_size[i] = acc_chunk_size[i - 1] + chunk_size[i - 1];
    }

    at::parallel_for(0, max_threads, 0, [&](int64_t start, int64_t end) {
      for (int64_t c = start; c < end; c++) {
        int64_t chunk_begin = acc_chunk_size[c];
        int64_t chunk_end = acc_chunk_size[c + 1];
        for (int64_t n = 0; n < sparse_nnz; n++) {
          int64_t chunk_offset = indices_accessor[0][n];
          if (chunk_offset >= chunk_begin && chunk_offset < chunk_end) {
            int64_t table_offset = 0;
            for (int64_t d = 0; d < sparse_dim; d++) {
              table_offset += sparse_stride[d] * indices_accessor[d][n];
            }
            auto value_index = value_ptr + n * feature_size;
            auto top_half_index = top_half_ptr + table_offset;
            auto bot_half_index = bot_half_ptr + table_offset;
            packed_bf16_add_ker(top_half_index, bot_half_index, value_index,
                                feature_size, alpha);
          }
        }
      }
    });
  } else {
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(grad.is_contiguous());
    // TODO: vector implementation basing on vector size
    union packed_bf16 {
      unsigned short s[2];
      float f;
    };

    auto len = top_half.numel();
    auto value_ptr = grad.data_ptr<at::BFloat16>();
    auto top_half_ptr = (unsigned short *)top_half.data_ptr();
    auto bot_half_ptr = (unsigned short *)bot_half.data_ptr();

    at::parallel_for(0, len, 0, [&](int64_t start, int64_t end) {
      for (int64_t i = start; i < end; i++) {
        packed_bf16 p16;
        p16.s[0] = bot_half_ptr[i];
        p16.s[1] = top_half_ptr[i];
        p16.f += alpha * (float)(value_ptr[i]);
        bot_half_ptr[i] = p16.s[0];
        top_half_ptr[i] = p16.s[1];
      }
    });
  }
}

template <typename T>
static inline void cat(const T *in1, const T *in2, T *out, size_t in1_size,
                       size_t in2_size) {
  std::memcpy(out, in1, in1_size * sizeof(T));
  std::memcpy(&out[in1_size], in2, in2_size * sizeof(T));
}

template <typename T>
static inline void cat_backward(const T *in, T *out1, T *out2, size_t out1_size,
                                size_t out2_size) {
  std::memcpy(out1, in, out1_size * sizeof(T));
  std::memcpy(out2, &in[out1_size], out2_size * sizeof(T));
}

template <typename T>
static inline void cat(T *out, const std::vector<T *> &in,
                       const std::vector<uint32_t> &feature_sizes, int64_t bs) {
  size_t offset = 0;
  for (int j = 0; j < feature_sizes.size(); j++) {
    std::memcpy(&out[offset], &in[j][bs * feature_sizes[j]],
                feature_sizes[j] * sizeof(T));
    offset += feature_sizes[j];
  }
}

template <typename T>
static inline void cat_backward(const T *in, std::vector<T *> &out,
                                const std::vector<uint32_t> &feature_sizes,
                                int64_t bs) {
  size_t offset = 0;
  for (int j = 0; j < feature_sizes.size(); j++) {
    std::memcpy(&out[j][bs * feature_sizes[j]], &in[offset],
                feature_sizes[j] * sizeof(T));
    offset += feature_sizes[j];
  }
}

template <typename T>
static inline void flat_triangle(const T *in, T *out, size_t size) {
  size_t offset = 0;
  for (int i = 1; i < size; i++) {
    std::memcpy(&out[offset], &in[i * size], i * sizeof(T));
    offset += i;
  }
}

template <typename T>
static inline void flat_triangle_backward(const T *in, T *out, size_t size) {
  size_t offset = 0;
  for (int i = 0; i < size * size; i++) {
    out[i] = 0.f;
  }
  for (int i = 1; i < size; i++) {
    std::memcpy(&out[i * size], &in[offset], i * sizeof(T));
    offset += i;
  }
}

template <typename T> static inline void add(const T *in, T *out, size_t size) {
#pragma omp simd
  for (size_t i = 0; i < size; i++) {
    out[i] += in[i];
  }
}

static inline void mm_backward(float *out, const float *in1, const float *in2,
                               uint32_t vector_nums, uint32_t vector_size,
                               libxsmm_smmfunction mm_ker) {
  // Calculate gy + gy'
  float sum_buf[vector_nums * vector_nums];
  for (int32_t j = 0; j < vector_nums; j++) {
    for (int32_t k = 0; k < vector_nums; k++) {
      sum_buf[j * vector_nums + k] =
          in1[j * vector_nums + k] + in1[k * vector_nums + j];
    }
  }
  // mm backward
  mm_ker(in2, sum_buf, out);
}

static inline void mm_backward(at::BFloat16 *out, const at::BFloat16 *in1,
                               const at::BFloat16 *in2, uint32_t vector_nums,
                               uint32_t vector_size,
                               libxsmm_smmfunction mm_ker) {
  float tmp_in1[vector_nums * vector_nums];
  float tmp_in2[vector_nums * vector_size];
  float tmp_out[vector_nums * vector_size];

  cvt_bf16_to_fp32(tmp_in1, in1, vector_nums * vector_nums);
  cvt_bf16_to_fp32(tmp_in2, in2, vector_nums * vector_size);
  // Calculate gy + gy'
  for (int32_t j = 0; j < vector_nums; j++) {
    for (int32_t k = 0; k < vector_nums; k++) {
      tmp_in1[j * vector_nums + k] += tmp_in1[k * vector_nums + j];
    }
  }
  // mm backward w/ fp32
  mm_ker(tmp_in2, tmp_in1, tmp_out);
  cvt_fp32_to_bf16(out, tmp_out, vector_nums * vector_size);
}

template <typename T>
inline at::Tensor _interaction_forward(const std::vector<at::Tensor> &input) {
#if defined(IPEX_PROFILE_OP)
  RECORD_FUNCTION("_interaction_forward", std::vector<c10::IValue>({input}));
#endif
  uint32_t total_feature_size = 0;
  int64_t batch_size = input[0].sizes()[0];
  uint32_t vector_size = input[0].sizes()[1];
  std::vector<uint32_t> feature_sizes(input.size());
  std::vector<T *> input_data(input.size());
  for (int i = 0; i < input.size(); i++) {
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(input[i].is_contiguous());
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(input[i].device().is_xpu());
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(input[i].dim() == 2);
    feature_sizes[i] = input[i].sizes()[1];
    total_feature_size += input[i].sizes()[1];
    input_data[i] = input[i].data_ptr<T>();
  }
  auto vector_nums = total_feature_size / vector_size;
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(total_feature_size % vector_size == 0);
  auto interact_feature_size = vector_nums * (vector_nums - 1) / 2;
  auto tr_vector_size = sizeof(T) == 4 ? vector_size : vector_size / 2;
  auto out = at::empty({batch_size, interact_feature_size + vector_size},
                       input[0].options());
  auto out_data = out.data_ptr<T>();

  auto mm_kernel = get_mm_kernel<T>(vector_nums, vector_nums, vector_size);
  auto tr_kernel = get_tr_kernel(tr_vector_size, vector_nums, vector_nums);

  at::parallel_for(0, batch_size, 0, [&](int64_t start, int64_t end) {
    T cat_buf[vector_nums * vector_size];
    T tr_buf[vector_nums * vector_size];
    T mm_buf[vector_nums * vector_nums];
    T flat_buf[interact_feature_size];
    for (int64_t i = start; i < end; i++) {
      cat<T>(cat_buf, input_data, feature_sizes, i);
      tr_kernel(cat_buf, &tr_vector_size, tr_buf, &vector_nums);
      mm_kernel((xsmm_dtype<T> *)tr_buf, (xsmm_dtype<T> *)cat_buf,
                (xsmm_dtype<T> *)mm_buf);
      flat_triangle<T>(mm_buf, flat_buf, vector_nums);
      cat<T>(&input_data[0][i * vector_size], flat_buf,
             &out_data[i * (interact_feature_size + vector_size)], vector_size,
             interact_feature_size);
    }
  });

  return out;
}

template <typename T>
inline std::vector<at::Tensor>
_interaction_backward(const at::Tensor &grad_out,
                      const std::vector<at::Tensor> &input) {
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(grad_out.is_contiguous());
#if defined(IPEX_PROFILE_OP)
  RECORD_FUNCTION("_interaction_backward",
                  std::vector<c10::IValue>({grad_out, input}));
#endif
  uint32_t total_feature_size = 0;
  int64_t batch_size = input[0].sizes()[0];
  uint32_t vector_size = input[0].sizes()[1];
  std::vector<uint32_t> feature_sizes(input.size());
  std::vector<at::Tensor> output(input.size());
  std::vector<T *> input_data(input.size());
  std::vector<T *> output_data(input.size());
  for (int i = 0; i < input.size(); i++) {
    auto feature_size = input[i].sizes()[1];
    feature_sizes[i] = feature_size;
    total_feature_size += feature_size;
    output[i] = at::empty({batch_size, feature_size}, input[i].options());
    input_data[i] = input[i].data_ptr<T>();
    output_data[i] = output[i].data_ptr<T>();
  }
  auto vector_nums = total_feature_size / vector_size;
  TORCH_INTERNAL_ASSERT_DEBUG_ONLY(total_feature_size % vector_size == 0);
  auto interact_feature_size = vector_nums * (vector_nums - 1) / 2;
  auto grad_out_data = grad_out.data_ptr<T>();

  auto mm_kernel = get_mm_kernel<float>(vector_nums, vector_size, vector_nums);

  at::parallel_for(0, batch_size, 0, [&](int64_t start, int64_t end) {
    T grad_input0_buf[vector_size];
    T grad_flat_buf[interact_feature_size];
    T grad_mm_buf[vector_nums * vector_nums];
    T grad_cat_buf[vector_nums * vector_size];
    T cat_buf[vector_nums * vector_size];
    for (int64_t i = start; i < end; i++) {
      cat_backward<T>(&grad_out_data[i * (interact_feature_size + vector_size)],
                      grad_input0_buf, grad_flat_buf, vector_size,
                      interact_feature_size);
      flat_triangle_backward<T>(grad_flat_buf, grad_mm_buf, vector_nums);

      // Special BMM characteristics in Interaction layer
      //  bmm(A, A'): two inputs are transposed to each other.
      //
      //             A --> (T) --> A'
      //              \         /
      //               \       /
      //                \     /
      //                 (bmm)
      //                   |
      //                   v
      //                  out
      //
      //  For traditional bmm backward propagation.
      //  e.g. gx: {gy, w'}, gw: {x', gy}
      //
      //  Can be expanded and optimized as:
      //  gx: {gy, A}, gA': {A', gy}
      //  gA = gx + (gA')' = {gy, A} + {A', gy}' = {gy + gy', A}

      // Calculate A
      cat<T>(cat_buf, input_data, feature_sizes, i);
      mm_backward(grad_cat_buf, grad_mm_buf, cat_buf, vector_nums, vector_size,
                  mm_kernel);
      cat_backward<T>(grad_cat_buf, output_data, feature_sizes, i);
      add<T>(grad_input0_buf, &output_data[0][i * vector_size], vector_size);
    }
  });
  return output;
}

at::Tensor
AtenIpexTypeExt::interaction_forward(const std::vector<at::Tensor> &input) {
  if (input[0].scalar_type() == at::kFloat) {
    for (auto &in : input) {
      cpu::dbl::comm::reorder_to_public(in);
      TORCH_INTERNAL_ASSERT_DEBUG_ONLY(in.scalar_type() == at::kFloat);
    }
    return _interaction_forward<float>(input);
  } else {
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(input[0].scalar_type() == at::kBFloat16);
    for (const auto &in : input) {
      TORCH_INTERNAL_ASSERT_DEBUG_ONLY(in.scalar_type() == at::kBFloat16);
    }
    return _interaction_forward<at::BFloat16>(input);
  }
}

std::vector<at::Tensor>
AtenIpexTypeExt::interaction_backward(const at::Tensor &grad_out,
                                      const std::vector<at::Tensor> &input) {
  if (grad_out.scalar_type() == at::kFloat) {
    cpu::dbl::comm::reorder_to_public(grad_out);
    return _interaction_backward<float>(grad_out, input);
  } else {
    TORCH_INTERNAL_ASSERT_DEBUG_ONLY(grad_out.scalar_type() == at::kBFloat16);
    return _interaction_backward<at::BFloat16>(grad_out, input);
  }
}

std::vector<at::Tensor> AtenIpexTypeExt::embedding_bag(
    const at::Tensor &weight, const at::Tensor &indices,
    const at::Tensor &offsets, bool scale_grad_by_freq, int64_t mode,
    bool sparse, const c10::optional<at::Tensor> &per_sample_weights,
    bool include_last_offset) {
  if (per_sample_weights.has_value()) {
    if (at::GradMode::is_enabled() && weight.requires_grad())
      return NewEmbeddingBagOp::apply(
          weight, indices, offsets, scale_grad_by_freq, mode, sparse,
          include_last_offset, per_sample_weights.value());
    return NewEmbeddingBagOp::_forward(
        weight, indices, offsets, scale_grad_by_freq, mode, sparse,
        include_last_offset, per_sample_weights.value());
  } else {
    if (at::GradMode::is_enabled() && weight.requires_grad())
      return NewEmbeddingBagOp::apply(weight, indices, offsets,
                                    scale_grad_by_freq, mode, sparse,
                                    include_last_offset);
    return NewEmbeddingBagOp::_forward(weight, indices, offsets,
                                    scale_grad_by_freq, mode, sparse,
                                    include_last_offset);
  }
}

at::Tensor AtenIpexTypeExt::linear(const at::Tensor &input,
                                   const at::Tensor &weight,
                                   const c10::optional<at::Tensor> &bias) {
  if (bias.has_value()) {
    if (at::GradMode::is_enabled() && weight.requires_grad())
      return NewLinearOp::apply(input, weight, bias.value());
    return NewLinearOp::_forward(input, weight, bias.value());
  } else {
    if (at::GradMode::is_enabled() && weight.requires_grad())
      return NewLinearOp::apply(input, weight);
    return NewLinearOp::_forward(input, weight);
  }
}

at::Tensor AtenIpexTypeExt::adaptive_avg_pool2d(at::Tensor const &input,
                                                at::IntArrayRef output_size) {
  if (at::GradMode::is_enabled())
    return NewApaptiveAvgPoolingOp::apply(input, output_size);
  return NewApaptiveAvgPoolingOp::_forward(input, output_size);
}

at::Tensor AtenIpexTypeExt::max_pool2d(const at::Tensor &input,
                                       at::IntArrayRef kernel_size,
                                       at::IntArrayRef stride,
                                       at::IntArrayRef padding,
                                       at::IntArrayRef dilation,
                                       bool ceil_mode) {
  if (at::GradMode::is_enabled())
    return NewMaxPool2dOp::apply(input, kernel_size, stride, padding, dilation,
                                 ceil_mode);
  auto ret = NewMaxPool2dOp::_forward(input, kernel_size, stride, padding,
                                      dilation, ceil_mode);
  return std::get<0>(ret);
}

at::Tensor AtenIpexTypeExt::max_pool3d(const at::Tensor &input,
                                       at::IntArrayRef kernel_size,
                                       at::IntArrayRef stride,
                                       at::IntArrayRef padding,
                                       at::IntArrayRef dilation,
                                       bool ceil_mode) {
  if (at::GradMode::is_enabled())
    return NewMaxPool3dOp::apply(input, kernel_size, stride, padding, dilation,
                                 ceil_mode);
  auto ret = NewMaxPool3dOp::_forward(input, kernel_size, stride, padding,
                                      dilation, ceil_mode);
  return std::get<0>(ret);
}


////////////////////////////////////////////////////////////////////////////////
// RNN OPS

std::vector<at::Tensor> rnn_layer(const at::Tensor& input,
    at::TensorList weights, const at::Tensor& hx,
    const at::Tensor& cx, bool reverse, int64_t mode,
    int64_t hidden_size, int64_t num_layers, bool train,
    bool bidirectional, at::IntArrayRef batch_sizes) {
  TORCH_CHECK(weights.size() == 2 || weights.size() == 4);
  if (weights.size() == 4) {
    if (at::GradMode::is_enabled())
      return NewRNNLayerOp::apply(input, weights[0], weights[1], weights[2], weights[3], hx, cx, reverse, mode, hidden_size, num_layers, true, train, bidirectional, batch_sizes);
    return NewRNNLayerOp::_forward(input, weights[0], weights[1], weights[2], weights[3], hx, cx, reverse, mode, hidden_size, num_layers, true, train, bidirectional, batch_sizes);
  } else {
    if (at::GradMode::is_enabled())
      return NewRNNLayerOp::apply(input, weights[0], weights[1], at::zeros(weights[0].sizes(), weights[0].options()), at::zeros(weights[1].sizes(), weights[1].options()), hx, cx, reverse, mode, hidden_size, num_layers, false, train, bidirectional, batch_sizes);
    return NewRNNLayerOp::_forward(input, weights[0], weights[1], at::zeros(weights[0].sizes(), weights[0].options()), at::zeros(weights[1].sizes(), weights[1].options()), hx, cx, reverse, mode, hidden_size, num_layers, false, train, bidirectional, batch_sizes);
  }
}
// MKLDNN RNN integration notes:
// I. Memory Formats
//   a. mkldnn will use plain formats for input, hx/cx, output, hy/cy
//      and possibly use blocked formats for weights depending shape info.
//   b. All mkldnn memorys are created (in plain format) as views on ATen tensor,
//      the weight reorder(if any) is handed automatically inside dil (mkldnn bridge)
//
// II. MKLDNN Primitive Mapping
//   a. mkldnn rnn primitive doesn't support training with dropout or padded input sequence.
//   b. here break a single RNN module into { num_layers * num_directions } mkldnn rnn primitives
//      for future need to cover these feature gaps.
//
//TODO: a. training with dropout
//   b. padded sequence input support
//
std::vector<at::Tensor> rnn(
    const at::Tensor& input_, std::vector<at::Tensor> weight, int64_t weight_stride0,
    const at::Tensor& hx_, const at::Tensor& cx_,
    int64_t mode, int64_t hidden_size,
    int64_t num_layers, bool batch_first, double dropout_p,
    bool train, bool bidirectional, at::IntArrayRef batch_sizes) {
  TORCH_CHECK(!train || dropout_p == 0.0, "mkldnn_rnn doesn't support dropout");
  TORCH_CHECK(batch_sizes.size() == 0, "mkldnn_rnn doesn't support packed input");

  auto input = input_;
  bool is_input_packed = batch_sizes.size() != 0;
  if (batch_first && !is_input_packed) {
    input = input.transpose(0, 1);
  }
  input = input.contiguous();

  auto hx = hx_.contiguous();
  auto cx = cx_.contiguous();

  at::MatrixRef<at::Tensor> weights{weight, static_cast<size_t>(weight_stride0)};

  auto num_directions = bidirectional ? 2 : 1;
  auto layer_input = input;
  std::vector<at::Tensor> layer_output(num_directions);
  std::vector<at::Tensor> layer_hy(num_layers * num_directions);
  std::vector<at::Tensor> layer_cy(num_layers * num_directions);
  for (int64_t layer = 0; layer < num_layers; layer++) {
    for (int64_t direction = 0; direction < num_directions; direction++) {
      auto index = layer * num_directions + direction;
      auto layer_weights = weights[index];
      auto layer_hx = hx[index];
      auto layer_cx = cx[index];
      auto reverse = (direction > 0);
      auto outputs = rnn_layer(layer_input, layer_weights, layer_hx, layer_cx, reverse, mode, hidden_size, num_layers, train, bidirectional, batch_sizes);
      layer_output[direction] = outputs[0];
      layer_hy[index] = outputs[1];
      layer_cy[index] = outputs[2];
    }
    layer_input = num_directions == 1 ? layer_output[0]
                                      : at::cat(layer_output, /*output_channels*/-1);
  }
  auto output = layer_input;
  auto hy = at::stack(layer_hy, 0);
  auto cy = at::stack(layer_cy, 0);

  if (batch_first && !is_input_packed) {
    output = output.transpose(0, 1);
  }

  return {output, hy, cy};
}

std::vector<at::Tensor> AtenIpexTypeExt::lstm(
    const at::Tensor& input, std::vector<at::Tensor> hidden, std::vector<at::Tensor> params, bool has_biases,
    int64_t num_layers, double dropout_p, bool train, bool bidirectional, bool batch_first) {
  at::Tensor hx = hidden[0];
  at::Tensor cx = hidden[1];
  int64_t hidden_size = hx.size(2);
  return rnn(
      input, params, has_biases ? 4 : 2,
      hx, cx, static_cast<int>(dil::rnn_kind::LSTM), hidden_size, num_layers, batch_first, dropout_p,
      train, bidirectional, /*batch_sizes*/{});

}

std::vector<at::Tensor> AtenIpexTypeExt::rnn_tanh(
    const at::Tensor& input, const at::Tensor& hidden, std::vector<at::Tensor> params, bool has_biases,
    int64_t num_layers, double dropout_p, bool train, bool bidirectional, bool batch_first) {
  at::Tensor hx = hidden;
  at::Tensor cx = at::zeros(hidden.sizes(), hidden.options());
  int64_t hidden_size = hx.size(2);
  auto outputs = rnn(
      input, params, has_biases ? 4 : 2,
      hx, cx, static_cast<int>(dil::rnn_kind::RNN_TANH), hidden_size, num_layers, batch_first, dropout_p,
      train, bidirectional, /*batch_sizes*/{});
  return {outputs[0], outputs[1]};
}

std::vector<at::Tensor> AtenIpexTypeExt::rnn_relu(
    const at::Tensor& input, const at::Tensor& hidden, std::vector<at::Tensor> params, bool has_biases,
    int64_t num_layers, double dropout_p, bool train, bool bidirectional, bool batch_first) {
  at::Tensor hx = hidden;
  at::Tensor cx = at::zeros(hidden.sizes(), hidden.options());
  int64_t hidden_size = hx.size(2);
  auto outputs = rnn(
      input, params, has_biases ? 4 : 2,
      hx, cx, static_cast<int>(dil::rnn_kind::RNN_RELU), hidden_size, num_layers, batch_first, dropout_p,
      train, bidirectional, /*batch_sizes*/{});
  return {outputs[0], outputs[1]};
}

std::vector<at::Tensor> AtenIpexTypeExt::gru(
    const at::Tensor& input, const at::Tensor& hidden, std::vector<at::Tensor> params, bool has_biases,
    int64_t num_layers, double dropout_p, bool train, bool bidirectional, bool batch_first) {
  at::Tensor hx = hidden;
  at::Tensor cx = at::zeros(hidden.sizes(), hidden.options());
  int64_t hidden_size = hx.size(2);
  auto outputs = rnn(
      input, params, has_biases ? 4 : 2,
      hx, cx, static_cast<int>(dil::rnn_kind::GRU), hidden_size, num_layers, batch_first, dropout_p,
      train, bidirectional, /*batch_sizes*/{});
  return {outputs[0], outputs[1]};
}

at::Tensor AtenIpexTypeExt::linear_relu(const at::Tensor &input,
                                   const at::Tensor &weight,
                                   const c10::optional<at::Tensor> &bias) {
  if (bias.has_value()) 
    return cpu::AtenIpexJITDev::dil_linear_fuse_eltwise(input, weight, bias.value(), dil::attr_t::fuse_relu());
  return cpu::AtenIpexJITDev::dil_linear_fuse_eltwise(input, weight, at::Tensor(), dil::attr_t::fuse_relu());
}

at::Tensor AtenIpexTypeExt::frozen_batch_norm(const at::Tensor& input, const at::Tensor& weight, const at::Tensor& bias, const at::Tensor& running_mean, const at::Tensor& running_var) {
  if (at::GradMode::is_enabled())
    return FrozenBatchNormOp::apply(input, weight, bias, running_mean, running_var);
  return FrozenBatchNormOp::_forward(input, weight, bias, running_mean, running_var);
}

} // namespace torch_ipex

namespace {
static auto dispatch =
    torch::RegisterOperators()
        .op("torch_ipex::linear", &torch_ipex::AtenIpexTypeExt::linear)
        .op("torch_ipex::linear_relu", &torch_ipex::AtenIpexTypeExt::linear_relu)
        .op("torch_ipex::max_pool2d",
            [](const at::Tensor &self, c10::List<int64_t> kernel_size,
               c10::List<int64_t> stride, c10::List<int64_t> padding,
               c10::List<int64_t> dilation, bool ceil_mode = false) {
              return torch_ipex::AtenIpexTypeExt::max_pool2d(
                  self, kernel_size.vec(), stride.vec(), padding.vec(),
                  dilation.vec(), ceil_mode);
            })
        .op("torch_ipex::max_pool3d",
            [](const at::Tensor &self, c10::List<int64_t> kernel_size,
               c10::List<int64_t> stride, c10::List<int64_t> padding,
               c10::List<int64_t> dilation, bool ceil_mode = false) {
              return torch_ipex::AtenIpexTypeExt::max_pool3d(
                  self, kernel_size.vec(), stride.vec(), padding.vec(),
                  dilation.vec(), ceil_mode);
            })
        .op("torch_ipex::adaptive_avg_pool2d",
            [](const at::Tensor &self, c10::List<int64_t> output_size) {
              return torch_ipex::AtenIpexTypeExt::adaptive_avg_pool2d(
                  self, output_size.vec());
            })
        .op("torch_ipex::embedding_bag",
            [](const at::Tensor &weight, const at::Tensor &indices,
               const at::Tensor &offsets, bool scale_grad_by_freq, int64_t mode,
               bool sparse, const c10::optional<at::Tensor> &per_sample_weights,
               bool include_last_offset) {
              return torch_ipex::AtenIpexTypeExt::embedding_bag(
                  weight, indices, offsets, scale_grad_by_freq, mode, sparse,
                  per_sample_weights, include_last_offset);
            })
        .op("torch_ipex::lstm",
            [](const at::Tensor& input, std::vector<at::Tensor> hidden, std::vector<at::Tensor> params, bool has_biases, int64_t num_layers, double dropout_p, bool train, bool bidirectional, bool batch_first) {
              return torch_ipex::AtenIpexTypeExt::lstm(input, hidden, params, has_biases, num_layers, dropout_p, train, bidirectional, batch_first);
            })
        .op("torch_ipex::rnn_tanh",
            [](const at::Tensor& input, const at::Tensor& hidden, std::vector<at::Tensor> params, bool has_biases, int64_t num_layers, double dropout_p, bool train, bool bidirectional, bool batch_first) {
              return torch_ipex::AtenIpexTypeExt::rnn_tanh(input, hidden, params, has_biases, num_layers, dropout_p, train, bidirectional, batch_first);
            })
        .op("torch_ipex::rnn_relu",
            [](const at::Tensor& input, const at::Tensor& hidden, std::vector<at::Tensor> params, bool has_biases, int64_t num_layers, double dropout_p, bool train, bool bidirectional, bool batch_first) {
              return torch_ipex::AtenIpexTypeExt::rnn_relu(input, hidden, params, has_biases, num_layers, dropout_p, train, bidirectional, batch_first);
            })
        .op("torch_ipex::gru",
            [](const at::Tensor& input, const at::Tensor& hidden, std::vector<at::Tensor> params, bool has_biases, int64_t num_layers, double dropout_p, bool train, bool bidirectional, bool batch_first) {
              return torch_ipex::AtenIpexTypeExt::gru(input, hidden, params, has_biases, num_layers, dropout_p, train, bidirectional, batch_first);
            })
        .op("torch_ipex::interaction_forward", &torch_ipex::AtenIpexTypeExt::interaction_forward)
        .op("torch_ipex::interaction_backward", &torch_ipex::AtenIpexTypeExt::interaction_backward)
        .op("torch_ipex::frozen_batch_norm", torch_ipex::AtenIpexTypeExt::frozen_batch_norm);
}