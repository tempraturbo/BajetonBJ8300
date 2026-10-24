// Post-script: para cada funcao-alvo (enderecos hex como argumentos), acha TODAS as chamadas por
// varredura crua (call jaddr23, como SeedCalls.java), garante que cada chamada esteja dentro de uma
// funcao (cria a partir do "push {rets}" mais proximo para tras se preciso), decompila e imprime as
// linhas do pseudocodigo que chamam o alvo, com os argumentos.
// Uso: -postScript CallSites.java 0x01e390ac 0x01e3a126 ...
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import java.util.*;

public class CallSites extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        Map<Function, String> cache = new HashMap<>();
        for (String arg : getScriptArgs()) {
            long target = Long.decode(arg);
            Function tf = getFunctionAt(toAddr(target));
            String tname = tf != null ? tf.getName() : String.format("FUN_%08x", target);
            List<Address> sites = new ArrayList<>();
            for (String bn : new String[] {"flash_app", "sram_code"}) {
                MemoryBlock b = mem.getBlock(bn);
                byte[] buf = new byte[(int) b.getSize()];
                b.getBytes(b.getStart(), buf);
                long base = b.getStart().getOffset();
                for (int i = 0; i + 3 < buf.length; i += 2) {
                    int b0 = buf[i] & 0xff, b1 = buf[i + 1] & 0xff;
                    if (b1 != 0xea || b0 < 0x80 || b0 > 0xbf) continue;
                    int disp = ((b0 & 0x3f) << 16) | ((buf[i + 3] & 0xff) << 8) | (buf[i + 2] & 0xff);
                    if ((disp & 0x200000) != 0) disp -= 0x400000;
                    if (base + i + 4 + 2L * disp == target) sites.add(toAddr(base + i));
                }
            }
            println(String.format("##### %s: %d chamadas", tname, sites.size()));
            Set<Function> done = new LinkedHashSet<>();
            for (Address site : sites) {
                Function f = getFunctionContaining(site);
                if (f == null) f = createAround(mem, site);
                if (f == null) { println("   " + site + ": sem funcao"); continue; }
                if (!done.add(f)) continue;
                String c = cache.computeIfAbsent(f, fn -> {
                    DecompileResults r = decomp.decompileFunction(fn, 120, monitor);
                    return r.decompileCompleted() ? r.getDecompiledFunction().getC() : "";
                });
                StringBuilder lines = new StringBuilder();
                for (String line : c.split("\n")) {
                    if (line.contains(tname + "(") || line.contains(String.format("func_0x%08x(", target))) {
                        lines.append("\n      ").append(line.strip());
                    }
                }
                println(String.format("   %s (chamada em %s):%s", f.getName(), site,
                        lines.length() == 0 ? " [chamada nao aparece no pseudocodigo]" : lines));
            }
        }
        decomp.dispose();
    }

    private Function createAround(Memory mem, Address site) throws Exception {
        for (int back = 0; back <= 4096; back += 2) {
            Address c = site.subtract(back);
            int hw = (mem.getByte(c) & 0xff) | ((mem.getByte(c.add(1)) & 0xff) << 8);
            if ((hw & 0xfff0) == 0x0470) {
                if (getFunctionContaining(c) != null) return null;
                disassemble(c);
                return createFunction(c, null);
            }
        }
        return null;
    }
}
