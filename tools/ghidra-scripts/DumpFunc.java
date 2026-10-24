// Desmonta e decompila as funcoes nos enderecos passados como argumentos do script.
// Uso (headless): -postScript DumpFunc.java [max:N] 0x01e00100 [0x...]
// max:N limita a listagem de instrucoes (0 = so pseudocodigo); usa ':' porque o .bat do Ghidra
// trata '=' como separador de argumentos.
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class DumpFunc extends GhidraScript {
    private int maxInsns = 160;

    @Override
    public void run() throws Exception {
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        for (String arg : getScriptArgs()) {
            if (arg.startsWith("max:")) {
                maxInsns = Integer.parseInt(arg.substring(4));
                continue;
            }
            Address addr = toAddr(arg);
            disassemble(addr);
            Function f = getFunctionContaining(addr);
            if (f == null) {
                f = createFunction(addr, null);
            }
            println("===== " + arg + " funcao=" + (f == null ? "nenhuma" : f.getName() + "@" + f.getEntryPoint()));
            Instruction ins = getInstructionAt(addr);
            for (int i = 0; ins != null && i < maxInsns; i++, ins = ins.getNext()) {
                StringBuilder bytes = new StringBuilder();
                for (byte b : ins.getBytes()) {
                    bytes.append(String.format("%02x", b));
                }
                println(String.format("  %s  %-12s %s", ins.getAddress(), bytes, ins));
            }
            if (f != null) {
                DecompileResults r = decomp.decompileFunction(f, 120, monitor);
                println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "decompilacao falhou: " + r.getErrorMessage());
            }
        }
        decomp.dispose();
    }
}
