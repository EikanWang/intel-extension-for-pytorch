#pragma once

#include <ATen/ATen.h>

#include "dil/dil.hpp"

namespace torch_ipex {
namespace cpu {

class AtenIpexCPUDev {
 public:
  static at::Tensor dil_convolution_overrideable(const at::Tensor & input, const at::Tensor & weight, const c10::optional<at::Tensor>& bias, at::IntArrayRef stride, at::IntArrayRef padding, at::IntArrayRef dilation, bool transposed, at::IntArrayRef output_padding, int64_t groups);
  static std::tuple<at::Tensor,at::Tensor,at::Tensor> dil_convolution_backward_overrideable(const at::Tensor & grad_output, const at::Tensor & input, const at::Tensor & weight, at::IntArrayRef stride, at::IntArrayRef padding, at::IntArrayRef dilation, bool transposed, at::IntArrayRef output_padding, int64_t groups, std::array<bool,3> output_mask);
  // aten::mkldnn_convolution(Tensor self, Tensor weight, Tensor? bias, int[] padding, int[] stride, int[] dilation, int groups) -> Tensor
  static std::tuple<at::Tensor,at::Tensor,at::Tensor> mkldnn_convolution_backward(const at::Tensor & self, const at::Tensor & grad_output, const at::Tensor & weight, at::IntArrayRef padding, at::IntArrayRef stride, at::IntArrayRef dilation, int64_t groups, std::array<bool,3> output_mask);
  static std::tuple<at::Tensor,at::Tensor,at::Tensor> cpu_deconvolution_backward(const at::Tensor & self, const at::Tensor & grad_output, const at::Tensor & weight, at::IntArrayRef padding, at::IntArrayRef output_padding, at::IntArrayRef stride, at::IntArrayRef dilation, int64_t groups, std::array<bool,3> output_mask);
  // For DNNL OPs
  static at::Tensor dil_convolution(const at::Tensor & input, const at::Tensor & weight, const at::Tensor & bias, at::IntArrayRef stride, at::IntArrayRef padding, at::IntArrayRef dilation, int64_t groups);
  static std::tuple<at::Tensor,at::Tensor,at::Tensor> dil_convolution_backward(const at::Tensor& input, const at::Tensor& grad_output_t, const at::Tensor& weight, at::IntArrayRef padding, at::IntArrayRef stride, at::IntArrayRef dilation, int64_t groups, std::array<bool,3> output_mask);
  static at::Tensor dil_deconvolution(const at::Tensor& input, const at::Tensor& weight, const at::Tensor& bias, at::IntArrayRef padding, at::IntArrayRef ouput_padding, at::IntArrayRef stride, at::IntArrayRef dilation, int64_t groups);
  static std::tuple<at::Tensor,at::Tensor,at::Tensor> dil_deconvolution_backward(const at::Tensor& input, const at::Tensor& grad_output_t, const at::Tensor& weight, at::IntArrayRef padding, at::IntArrayRef output_padding, at::IntArrayRef stride, at::IntArrayRef dilation, int64_t groups, std::array<bool,3> output_mask);
  static at::Tensor& dil_add_out(at::Tensor& result, const at::Tensor& self, const at::Tensor& other, at::Scalar alpha);
  static at::Tensor dil_add(const at::Tensor& self, const at::Tensor& other, at::Scalar alpha);
  static at::Tensor & dil_add_(at::Tensor & self, const at::Tensor & other, at::Scalar alpha);
  static at::Tensor& dil_mul_out(at::Tensor& result, const at::Tensor& self, const at::Tensor& other);
  static at::Tensor dil_mul(const at::Tensor& self, const at::Tensor& other);
  static at::Tensor & dil_mul_(at::Tensor & self, const at::Tensor & other);
  static at::Tensor dil_bmm(const at::Tensor& self, const at::Tensor& mat2);
  static at::Tensor& dil_bmm_out(at::Tensor &result, const at::Tensor& batch1, const at::Tensor& batch2);
  static at::Tensor dil_mm(const at::Tensor& self, const at::Tensor& mat2);
  static at::Tensor& dil_mm_out(at::Tensor& result, const at::Tensor& self, const at::Tensor& mat2);
  static at::Tensor dil_baddbmm(const at::Tensor& self, const at::Tensor& batch1, const at::Tensor & batch2, at::Scalar beta, at::Scalar alpha);
  static at::Tensor& dil_baddbmm_(at::Tensor& self, const at::Tensor& batch1, const at::Tensor& batch2, at::Scalar beta, at::Scalar alpha);
  static at::Tensor& baddbmm_common(at::Tensor &result, const dil::tensor &bias, const dil::tensor &x, const dil::tensor &w, at::Scalar beta, at::Scalar alpha);
  static at::Tensor& dil_baddbmm_out(at::Tensor &result, const at::Tensor& self, const at::Tensor& batch1, const at::Tensor& batch2, at::Scalar beta, at::Scalar alpha);
  static at::Tensor dil_addmm(const at::Tensor& self, const at::Tensor& batch1, const at::Tensor & batch2, at::Scalar beta, at::Scalar alpha);
  static at::Tensor& dil_addmm_(at::Tensor& self, const at::Tensor& batch1, const at::Tensor & batch2, at::Scalar beta, at::Scalar alpha);
  static at::Tensor& dil_addmm_out(at::Tensor& result, const at::Tensor& self, const at::Tensor& mat1, const at::Tensor& mat2, at::Scalar beta, at::Scalar alpha);
  static at::Tensor dil_addbmm(const at::Tensor &self, const at::Tensor &batch1, const at::Tensor &batch2, at::Scalar beta, at::Scalar alpha);
  static at::Tensor& dil_addbmm_(at::Tensor& self, const at::Tensor& batch1, const at::Tensor& batch2, at::Scalar beta, at::Scalar alpha);
  static at::Tensor& dil_addbmm_out(at::Tensor& result, const at::Tensor &self, const at::Tensor &batch1, const at::Tensor &batch2, at::Scalar beta, at::Scalar alpha);
  static at::Tensor dil_linear(const at::Tensor& self, const at::Tensor& weight, const at::Tensor& bias, const dil::attr_t& attr=dil::attr_t());
  static at::Tensor dil_linear_backward_input(at::IntArrayRef input_size, const at::Tensor& grad_output, const at::Tensor& weight);
  static std::tuple<at::Tensor, at::Tensor> dil_linear_backward_weights(const at::Tensor& grad_output, const at::Tensor& input, const at::Tensor& weight, bool bias_defined);
  static std::tuple<at::Tensor, at::Tensor, at::Tensor> dil_linear_backward(const at::Tensor& input, const at::Tensor& grad_output, const at::Tensor& weight, std::array<bool,3> output_mask);
  static at::Tensor dil_dropout(const at::Tensor& self, double ratio, bool train);
  static at::Tensor dil_dropout_backward(const at::Tensor& grady, const at::Tensor& mask, double ratio);
  static std::tuple<at::Tensor, at::Tensor, at::Tensor> dil_native_batch_norm(const at::Tensor& input, const at::Tensor& weight, const at::Tensor& bias, const at::Tensor& running_mean, const at::Tensor& running_var, bool train, double momentum, double eps);
  static std::tuple<at::Tensor, at::Tensor, at::Tensor> dil_native_batch_norm_backward(const at::Tensor& grad_output, const at::Tensor& input, const at::Tensor& weight, const at::Tensor& running_mean, const at::Tensor& running_var, const at::Tensor& save_mean, const at::Tensor& save_invstd, bool train,double eps, std::array<bool,3> grad_input_mask);
  static at::Tensor dil_frozen_batch_norm(const at::Tensor& input, const at::Tensor& weight, const at::Tensor& bias, const at::Tensor& running_mean, const at::Tensor& running_var, double eps);
  static at::Tensor dil_frozen_batch_norm_backward(const at::Tensor& grad_output, const at::Tensor& input, const at::Tensor& weight, const at::Tensor& running_mean, const at::Tensor& running_var, double eps);
  static at::Tensor dil_max_pooling(const at::Tensor& input, at::IntArrayRef kernel_size, at::IntArrayRef stride, at::IntArrayRef padding, at::IntArrayRef dilation, bool ceil_mode);
  static at::Tensor dil_avg_pool2d(const at::Tensor& input, at::IntArrayRef kernel_size, at::IntArrayRef stride, at::IntArrayRef padding, bool ceil_mode, bool count_include_pad, c10::optional<int64_t> divisor_override);
  static at::Tensor dil_avg_pool3d(const at::Tensor& input, at::IntArrayRef kernel_size, at::IntArrayRef stride, at::IntArrayRef padding, bool ceil_mode, bool count_include_pad, c10::optional<int64_t> divisor_override);
  static at::Tensor dil_adaptive_avg_pool2d(at::Tensor const& input, at::IntArrayRef output_size);
  static at::Tensor dil_max_pooling_backward(const at::Tensor& grad_output, const at::Tensor& output, const at::Tensor& input, at::IntArrayRef kernel_size, at::IntArrayRef stride, at::IntArrayRef padding, at::IntArrayRef dilation, bool ceil_mode);
  static at::Tensor dil_avg_pool2d_backward(const at::Tensor& grad_output, const at::Tensor& input, at::IntArrayRef kernel_size, at::IntArrayRef stride, at::IntArrayRef padding, bool ceil_mode, bool count_include_pad, c10::optional<int64_t> divisor_override);
  static at::Tensor dil_avg_pool3d_backward(const at::Tensor& grad_output, const at::Tensor& input, at::IntArrayRef kernel_size, at::IntArrayRef stride, at::IntArrayRef padding, bool ceil_mode, bool count_include_pad, c10::optional<int64_t> divisor_override);
  static at::Tensor dil_adaptive_avg_pool2d_backward(const at::Tensor& grad_output, const at::Tensor& input);
  static at::Tensor dil_relu(const at::Tensor& input);
  static at::Tensor& dil_relu_(at::Tensor& input);
  static at::Tensor dil_threshold_backward(const at::Tensor& grad_output, const at::Tensor& input, at::Scalar threshold);
  static at::Tensor dil__softmax(const at::Tensor& self, const int64_t dim, bool half_to_float);
  static at::Tensor dil__softmax_backward_data(const at::Tensor& grad_output, const at::Tensor& output, int64_t dim, const at::Tensor& self);
  static at::Tensor dil__log_softmax(const at::Tensor& self, const int64_t dim, bool half_to_float);
  static at::Tensor dil__log_softmax_backward_data(const at::Tensor& grad_output, const at::Tensor& output, int64_t dim, const at::Tensor& self);
  static at::Tensor dil_sigmoid(const at::Tensor& self);
  static at::Tensor& dil_sigmoid_(at::Tensor& self);
  static at::Tensor dil_sigmoid_backward(const at::Tensor& grad_output, const at::Tensor& output);
  static at::Tensor dil_tanh(const at::Tensor& self);
  static at::Tensor& dil_tanh_(at::Tensor& self);
  static at::Tensor dil_tanh_backward(const at::Tensor& grad_output, const at::Tensor& output);
  static at::Tensor dil_reshape(const at::Tensor& self, at::IntArrayRef size);
  static int64_t dil_size(const at::Tensor & self, int64_t dim);
  static at::Tensor dil_clone(const at::Tensor& self, c10::optional<c10::MemoryFormat> optional_memory_format);
  static at::Tensor dil_transpose(const at::Tensor & self, int64_t dim0, int64_t dim1);
  static at::Tensor& dil_cat_out(at::Tensor& result, at::TensorList tensors, int64_t dim);
  static at::Tensor dil_cat(at::TensorList tensors, int64_t dim);
  static std::vector<at::Tensor> dil_split_with_sizes(const at::Tensor& self, at::IntArrayRef split_sizes, int64_t dim);
  static std::vector<at::Tensor> dil_split(const at::Tensor& self, int64_t split_size, int64_t dim);
  static at::Tensor dil_gelu(const at::Tensor& input);
  static at::Tensor dil_gelu_backward(const at::Tensor& grad_output, const at::Tensor& input);
  static std::tuple<at::Tensor, at::Tensor, at::Tensor> dil_native_layer_norm(const at::Tensor& X, const at::Tensor& gamma, const at::Tensor& beta, int64_t M, int64_t N, double eps);
  static std::tuple<at::Tensor, at::Tensor, at::Tensor> dil_native_layer_norm(const at::Tensor& input, at::IntArrayRef normalized_shape, const c10::optional<at::Tensor> & weight, const c10::optional<at::Tensor> & bias, double eps);
  static std::tuple<at::Tensor, at::Tensor, at::Tensor> dil_native_layer_norm_backward(const at::Tensor& dY, const at::Tensor& X, const at::Tensor& mean, const at::Tensor& rstd, const at::Tensor& gamma, int64_t M, int64_t N, std::array<bool, 3> grad_input_mask);
  static std::tuple<at::Tensor, at::Tensor, at::Tensor> dil_native_layer_norm_backward(const at::Tensor & grad_out, const at::Tensor & input, at::IntArrayRef normalized_shape, const at::Tensor & mean, const at::Tensor & rstd, const c10::optional<at::Tensor> & weight, const c10::optional<at::Tensor> & bias, std::array<bool,3> output_mask);
  static at::Tensor dil_slice(const at::Tensor & self, int64_t dim, int64_t start, int64_t end, int64_t step);
  static at::Tensor dil_slice(const at::Tensor & self, int64_t dim, c10::optional<int64_t> start, c10::optional<int64_t> end, int64_t step);
  static std::vector<at::Tensor> dil_unbind(const at::Tensor &self, int64_t dim);
  static std::vector<at::Tensor> dil_unbind(const at::Tensor& self, at::Dimname dim);
  static at::Tensor dil_select(const at::Tensor & self, int64_t dim, int64_t index);
  static at::Tensor dil_select(const at::Tensor & self, at::Dimname dim, int64_t index);
  static at::Tensor dil_view(const at::Tensor & self, at::IntArrayRef size);
  static at::Tensor dil_index_select(const at::Tensor & self, int64_t dim, const at::Tensor & index);
  static at::Tensor dil_index(const at::Tensor & self, const c10::List<c10::optional<at::Tensor>> & indices);
  static at::Tensor dil__unsafe_view(const at::Tensor & self, at::IntArrayRef size);
  static at::Tensor dil_shuffle(const at::Tensor & self, at::IntArrayRef view_shape, int64_t dim0, int64_t dim1);
  static std::tuple<at::Tensor,at::Tensor> dil__pack_padded_sequence(const at::Tensor & input, const at::Tensor & lengths, bool batch_first);
  static at::Tensor& dil_copy_(at::Tensor & self, const at::Tensor & src, bool non_blocking);
  static std::vector<at::Tensor> dil_rnn_layer(const at::Tensor& input, const at::Tensor& w1, const at::Tensor& w2, const at::Tensor& w3, const at::Tensor& w4, const at::Tensor& hx, const at::Tensor& cx, bool reverse, int64_t mode, int64_t hidden_size, int64_t num_layers, bool has_biases, bool train, bool bidirectional, at::IntArrayRef batch_sizes);
  static std::vector<at::Tensor> dil_rnn_layer_backward(const at::Tensor& input, const at::Tensor& w1, const at::Tensor& w2, const at::Tensor& w3, const at::Tensor& w4, const at::Tensor& hx, const at::Tensor& cx, const at::Tensor& output, const at::Tensor& hy, const at::Tensor& cy, const at::Tensor& grad_output, const at::Tensor& grad_hy, const at::Tensor& grad_cy, bool reverse, int64_t mode, int64_t hidden_size, int64_t num_layers, bool has_biases, bool train, bool bidirectional, at::IntArrayRef batch_sizes);
  static at::Tensor dil_upsample_nearest1d(const at::Tensor & self, at::IntArrayRef output_size, c10::optional<double> scales);
  static at::Tensor dil_upsample_nearest1d_backward(const at::Tensor & grad_output, at::IntArrayRef output_size, at::IntArrayRef input_size, c10::optional<double> scales);
  static at::Tensor dil_upsample_nearest2d(const at::Tensor& input, at::IntArrayRef output_size, c10::optional<double> scales_h, c10::optional<double> scales_w);
  static at::Tensor dil_upsample_nearest2d_backward(const at::Tensor& grad_output, at::IntArrayRef output_size, at::IntArrayRef input_size, c10::optional<double> scales_h, c10::optional<double> scales_w);
  static at::Tensor dil_upsample_nearest3d(const at::Tensor & self, at::IntArrayRef output_size, c10::optional<double> scales_d, c10::optional<double> scales_h, c10::optional<double> scales_w);
  static at::Tensor dil_upsample_nearest3d_backward(const at::Tensor & grad_output, at::IntArrayRef output_size, at::IntArrayRef input_size, c10::optional<double> scales_d, c10::optional<double> scales_h, c10::optional<double> scales_w);
  static at::Tensor dil_upsample_linear1d(const at::Tensor & self, at::IntArrayRef output_size, bool align_corners, c10::optional<double> scales);
  static at::Tensor dil_upsample_linear1d_backward(const at::Tensor & grad_output, at::IntArrayRef output_size, at::IntArrayRef input_size, bool align_corners, c10::optional<double> scales);
  static at::Tensor dil_upsample_bilinear2d(const at::Tensor & self, at::IntArrayRef output_size, bool align_corners, c10::optional<double> scales_h, c10::optional<double> scales_w);
  static at::Tensor dil_upsample_bilinear2d_backward(const at::Tensor & grad_output, at::IntArrayRef output_size, at::IntArrayRef input_size, bool align_corners, c10::optional<double> scales_h, c10::optional<double> scales_w);
  static at::Tensor dil_upsample_trilinear3d(const at::Tensor & self, at::IntArrayRef output_size, bool align_corners, c10::optional<double> scales_d, c10::optional<double> scales_h, c10::optional<double> scales_w);
  static at::Tensor dil_upsample_trilinear3d_backward(const at::Tensor & grad_output, at::IntArrayRef output_size, at::IntArrayRef input_size, bool align_corners, c10::optional<double> scales_d, c10::optional<double> scales_h, c10::optional<double> scales_w);
  static at::Tensor dil_unsqueeze(const at::Tensor& self, int64_t dim);
  static at::Tensor dil_div(const at::Tensor &self, const at::Tensor &other);
  static at::Tensor dil_div(const at::Tensor &self, const at::Scalar &other);
  static at::Tensor &dil_div_(at::Tensor &self, const at::Tensor &other);
  static at::Tensor &dil_div_(at::Tensor &self, const at::Scalar &other);
  static at::Tensor &dil_div_out(at::Tensor &out, const at::Tensor &self,
                                 const at::Tensor &other);
  static at::Tensor dil_permute(const at::Tensor & self, at::IntArrayRef dims);
};

}  // namespace cpu
}  // namespace torch_ipex
