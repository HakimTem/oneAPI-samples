#include <sycl/ext/intel/fpga_extensions.hpp>
#include <sycl/sycl.hpp>

#include "exception_handler.hpp"

constexpr int kBL1 = 1;
constexpr int kBL2 = 2;
constexpr int kBL3 = 3;
constexpr int kAlignment = 4;

struct MultiMMIP {
  // Each annotated pointer is configured with a unique `buffer_location`,
  // resulting in three unique Avalon memory-mapped host interfaces.
  using XProps = decltype(sycl::ext::oneapi::experimental::properties{
      sycl::ext::intel::experimental::buffer_location<kBL1>,
      sycl::ext::intel::experimental::awidth<32>,
      sycl::ext::intel::experimental::dwidth<32>,
      sycl::ext::intel::experimental::latency<1>,
      sycl::ext::oneapi::experimental::alignment<kAlignment>,
      sycl::ext::intel::experimental::read_write_mode_read});
  using YProps = decltype(sycl::ext::oneapi::experimental::properties{
      sycl::ext::intel::experimental::buffer_location<kBL2>,
      sycl::ext::intel::experimental::awidth<32>,
      sycl::ext::intel::experimental::dwidth<32>,
      sycl::ext::intel::experimental::latency<1>,
      sycl::ext::oneapi::experimental::alignment<kAlignment>,
      sycl::ext::intel::experimental::read_write_mode_read});
  using ZProps = decltype(sycl::ext::oneapi::experimental::properties{
      sycl::ext::intel::experimental::buffer_location<kBL3>,
      sycl::ext::intel::experimental::awidth<32>,
      sycl::ext::intel::experimental::dwidth<32>,
      sycl::ext::intel::experimental::latency<1>,
      sycl::ext::oneapi::experimental::alignment<kAlignment>,
      sycl::ext::intel::experimental::read_write_mode_write});

  sycl::ext::oneapi::experimental::annotated_arg<int *, XProps> x;
  sycl::ext::oneapi::experimental::annotated_arg<int *, YProps> y;
  sycl::ext::oneapi::experimental::annotated_arg<int *, ZProps> z;

  int size;

  void operator()() const {
#pragma unroll 4
    for (int i = 0; i < size; i++) {
      z[i] = x[i] + y[i];
    }
  }
};

int main(void) {
#if FPGA_SIMULATOR
  auto selector = sycl::ext::intel::fpga_simulator_selector_v;
#elif FPGA_HARDWARE
  auto selector = sycl::ext::intel::fpga_selector_v;
#else  // #if FPGA_EMULATOR
  auto selector = sycl::ext::intel::fpga_emulator_selector_v;
#endif

  bool passed = true;

  try {
    // create the device queue
    sycl::queue q(selector, fpga_tools::exception_handler);

    // Print out the device information.
    sycl::device device = q.get_device();
    std::cout << "Running on device: "
              << q.get_device().get_info<sycl::info::device::name>().c_str()
              << std::endl;

    // Create and initialize the host arrays
    constexpr int kN = 8;
    std::cout << "Elements in vector : " << kN << "\n";

    // Host array must share the same buffer location property as defined in the
    // kernel. Since we are specifiying alignment on the kernal argument, we
    // need to also specify that to the allocation call by using
    // alighned_alloc_shared API
    int *array_a = sycl::aligned_alloc_shared<int>(
        kAlignment, kN, q,
        sycl::ext::intel::experimental::property::usm::buffer_location(kBL1));
    int *array_b = sycl::aligned_alloc_shared<int>(
        kAlignment, kN, q,
        sycl::ext::intel::experimental::property::usm::buffer_location(kBL2));
    int *array_c = sycl::aligned_alloc_shared<int>(
        kAlignment, kN, q,
        sycl::ext::intel::experimental::property::usm::buffer_location(kBL3));

    assert(array_a);
    assert(array_b);
    assert(array_c);

    for (int i = 0; i < kN; i++) {
      array_a[i] = i;
      array_b[i] = 2 * i;
    }

    q.single_task(MultiMMIP{array_a, array_b, array_c, kN}).wait();
    for (int i = 0; i < kN; i++) {
      auto golden = 3 * i;
      if (array_c[i] != golden) {
        std::cout << "ERROR! At index: " << i << " , expected: " << golden
                  << " , found: " << array_c[i] << "\n";
        passed = false;
      }
    }

    std::cout << (passed ? "PASSED" : "FAILED") << std::endl;

    free(array_a, q);
    free(array_b, q);
    free(array_c, q);

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
  } catch (sycl::exception const &e) {
    // Catches exceptions in the host code
    std::cerr << "Caught a SYCL host exception:\n" << e.what() << "\n";

    // Most likely the runtime couldn't find FPGA hardware!
    if (e.code().value() == CL_DEVICE_NOT_FOUND) {
      std::cerr << "If you are targeting an FPGA, please ensure that your "
                   "system has a correctly configured FPGA board.\n";
      std::cerr << "Run sys_check in the oneAPI root directory to verify.\n";
      std::cerr << "If you are targeting the FPGA emulator, compile with "
                   "-DFPGA_EMULATOR.\n";
    }
    std::terminate();
  }
}