// Post-script: para cada endereco (tipicamente um call fora de funcao conhecida), acha o inicio
// provavel da funcao (o "push {rets,...}" = meia-palavra 0x047N mais proximo para tras, ate 4 KB),
// cria a funcao, e imprime pseudocodigo + chamadores. Uso: -postScript Context.java 0x... [0x...]
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;

public class Context extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        for (String arg : getScriptArgs()) {
            Address a = toAddr(arg);
            Function f = getFunctionContaining(a);
            if (f == null) {
                Address start = null;
                for (int back = 0; back <= 4096; back += 2) {
                    Address c = a.subtract(back);
                    int hw = (mem.getByte(c) & 0xff) | ((mem.getByte(c.add(1)) & 0xff) << 8);
                    if ((hw & 0xfff0) == 0x0470) { start = c; break; }
                }
                if (start == null) { println(arg + ": sem push {rets} ate 4 KB para tras"); continue; }
                clearListing(start, a.add(64));
                disassemble(start);
                f = createFunction(start, null);
            }
            if (f == null) { println(arg + ": nao consegui criar funcao"); continue; }
            StringBuilder callers = new StringBuilder();
            for (Function c : f.getCallingFunctions(monitor)) callers.append(c.getName()).append(' ');
            println("===== " + arg + " dentro de " + f.getName() + "@" + f.getEntryPoint()
                    + " chamadores: " + (callers.length() == 0 ? "nenhum conhecido" : callers));
            DecompileResults r = decomp.decompileFunction(f, 120, monitor);
            println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "decompilacao falhou: " + r.getErrorMessage());
        }
        decomp.dispose();
    }
}
