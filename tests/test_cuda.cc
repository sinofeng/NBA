#include <cstdint>
#include <cuda_runtime.h>
#include <nba/core/shiftedint.hh>
#include <nba/framework/datablock_shared.hh>
#include <gtest/gtest.h>
#include <nba/engines/cuda/test.hh>
#if 0
#require <engines/cuda/test.o>
#endif

using namespace std;
using namespace nba;

#ifdef USE_CUDA

TEST(CUDADeviceTest, Initialization) {
    ASSERT_EQ(cudaSuccess, cudaSetDevice(0));
    ASSERT_EQ(cudaSuccess, cudaDeviceReset());
}

TEST(CUDADeviceTest, NoopKernel) {
    ASSERT_EQ(cudaSuccess, cudaSetDevice(0));
    void *k = get_test_kernel_noop();
    EXPECT_EQ(cudaSuccess, cudaLaunchKernel(k, dim3(1), dim3(1), nullptr, 0, 0));
    EXPECT_EQ(cudaSuccess, cudaDeviceSynchronize());
    ASSERT_EQ(cudaSuccess, cudaDeviceReset());
}

class CUDAStructTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        cudaSetDevice(0);
    }

    virtual void TearDown() {
        cudaDeviceReset();
    }
};

TEST(CUDAStructTest, ShfitedIntSizeCheck) {
    void *k = get_test_kernel_shiftedint_size_check();
    void *output_d;
    ASSERT_EQ(cudaSuccess, cudaMalloc(&output_d, sizeof(size_t)));
    ASSERT_NE(nullptr, output_d);
    size_t output_h = 0;
    void *raw_args[1] = { &output_d };
    EXPECT_EQ(cudaSuccess, cudaLaunchKernel(k, dim3(1), dim3(1), raw_args, 0, 0));
    EXPECT_EQ(cudaSuccess, cudaMemcpy(&output_h, output_d, sizeof(size_t), cudaMemcpyDeviceToHost));
    EXPECT_EQ(sizeof(nba::dev_offset_t), 2);
    EXPECT_EQ(sizeof(nba::dev_offset_t), output_h);
    EXPECT_EQ(cudaSuccess, cudaFree(output_d));
    EXPECT_EQ(cudaSuccess, cudaDeviceSynchronize());
}

TEST(CUDAStructTest, ShfitedIntValueCheck) {
    void *k = get_test_kernel_shiftedint_value_check();
    void *input_d;
    void *output_d;
    ASSERT_EQ(cudaSuccess, cudaMalloc(&input_d, sizeof(nba::dev_offset_t)));
    ASSERT_NE(nullptr, input_d);
    ASSERT_EQ(cudaSuccess, cudaMalloc(&output_d, sizeof(uint64_t)));
    ASSERT_NE(nullptr, output_d);
    nba::dev_offset_t input_h = 165321;
    EXPECT_EQ(165320, input_h.as_value<uint64_t>());
    size_t output_h = 0;
    void *raw_args[2] = { &input_d, &output_d };
    EXPECT_EQ(cudaSuccess, cudaMemcpy(input_d, &input_h, sizeof(nba::dev_offset_t), cudaMemcpyHostToDevice));
    EXPECT_EQ(cudaSuccess, cudaLaunchKernel(k, dim3(1), dim3(1), raw_args, 0, 0));
    EXPECT_EQ(cudaSuccess, cudaMemcpy(&output_h, output_d, sizeof(uint64_t), cudaMemcpyDeviceToHost));
    EXPECT_EQ(165320, output_h);
    EXPECT_EQ(cudaSuccess, cudaFree(input_d));
    EXPECT_EQ(cudaSuccess, cudaFree(output_d));
    EXPECT_EQ(cudaSuccess, cudaDeviceSynchronize());
}

#else

TEST(CUDATest, Noop) {
    EXPECT_TRUE(1);
}

#endif

// vim: ts=8 sts=4 sw=4 et
