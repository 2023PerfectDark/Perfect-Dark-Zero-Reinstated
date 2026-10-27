#include <rex/codegen/codegen_context.h>
#include <rex/codegen/function_node.h>
#include <rex/codegen/test_support.h>
#include <fstream>
#include <map>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 2) return 1;
  // Real PPC instructions: root and middle keep nonvolatile values on their
  // native C++ frames. The leaf changes a guest register before a safe point.
  const uint32_t words[] = {
    0x39c0000b, 0x38a50001, 0x48000019, 0x7c637214, 0x4e800020,
    0x60000000, 0x60000000, 0x60000000,
    0x39e00004, 0x3d608200, 0x616b1040, 0x7d6903a6,
    0x4e800421, 0x7c6379d6, 0x4e800020, 0x60000000,
    0x38c60007, 0x39600000, 0x7d6903a6, 0x48000015,
    0x38660003, 0x4e800020, 0x60000000, 0x60000000,
    0x4e800020
  };
  std::vector<uint8_t> bytes;
  for (auto word : words) for (int shift = 24; shift >= 0; shift -= 8)
    bytes.push_back(uint8_t(word >> shift));
  using namespace rex::codegen;
  TestModule module; module.Load(0x82001000, bytes.data(), bytes.size());
  RecompilerConfig config;
  config.checkpointContinuations = true;
  config.nonVolatileRegistersAsLocalVariables = true;
  config.nonArgumentRegistersAsLocalVariables = true;
  config.crRegistersAsLocalVariables = true;
  auto context = CodegenContext::Create(BinaryView::fromModule(module), config);
  std::map<size_t, std::string> symbols{{0x82001000,"test_root"},{0x82001020,"test_middle"},
                                      {0x82001040,"test_leaf"},{0x82001060,"test_end"}};
  AnalyzeTestBinary(context, "checkpoint", symbols, 0x82001000, bytes.data(), bytes.size());
  EmitContext emit{context.binary(), context.Config(), context.graph};
  std::ofstream output(argv[1]);
  output << "#include <rex/ppc/context.h>\n"
            "#define DEFINE_REX_FUNC(name) void name(PPCContext& ctx, uint8_t* base)\n"
            "#define REX_FUNC_PROLOGUE()\n"
            "void CheckpointIndirect(uint32_t, PPCContext&, uint8_t*);\n"
            "#define REX_CALL_INDIRECT_FUNC(address) CheckpointIndirect(address, ctx, base)\n";
  for (auto [address, name] : symbols)
    output << "void checkpoint_" << std::hex << std::uppercase << address
           << "(PPCContext&, uint8_t*);\n";
  for (auto [address, name] : symbols) output << context.graph.getFunction(address)->emitCpp(emit);
  return output ? 0 : 1;
}
