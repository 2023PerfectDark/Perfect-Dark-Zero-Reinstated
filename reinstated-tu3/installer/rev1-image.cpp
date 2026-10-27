// Installer-only reader. Never executes guest code; emits the verified mapped
// Rev 1 image for the version-specific compatibility delta.
#include <rex/runtime.h>
#include <rex/kernel/init.h>
#include <rex/system/user_module.h>
#include <rex/system/xex_module.h>
#include <fstream>
int main(int argc, char** argv) {
  if (argc != 3) return 2;
  const auto assets = std::filesystem::absolute(argv[1]);
  const auto output = std::filesystem::absolute(argv[2]);
  if (std::filesystem::exists(assets / "default.xexp")) return 3;
  rex::Runtime runtime(assets, output.parent_path() / "reader-user");
  if (runtime.Setup(rex::RuntimeConfig{
          .kernel_init = rex::kernel::InitializeKernel, .tool_mode = true})) return 4;
  if (runtime.LoadXexImage("game:\\default.xex")) return 5;
  auto* xex = runtime.kernel_state()->GetExecutableModule()->xex_module();
  if (xex->base_address() != 0x82000000 || xex->image_size() != 0x013E0000) return 6;
  std::ofstream stream(output, std::ios::binary);
  stream.write(reinterpret_cast<const char*>(runtime.memory()->TranslateVirtual(
                   xex->base_address())), xex->image_size());
  stream.close();
  return stream.good() ? 0 : 7;
}
