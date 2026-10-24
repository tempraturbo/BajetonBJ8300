// Pre-script: semeia funcoes nos destinos das instrucoes "call" do pi32v2 (jaddr23), achadas por
// varredura de bytes, como em dank0i/cyclone2. O auto-analysis sozinho cobre ~30% do app.bin.
// Codificacao (conferida em 6 chamadas da rotina de partida):
//   bytes b0 b1 b2 b3 com b1 == 0xEA e 0x80 <= b0 <= 0xBF
//   disp = ((b0 & 0x3F) << 16) | (b3 << 8 | b2), com sinal em 22 bits, em meias-palavras
//   destino = endereco + 4 + 2*disp
// Para reduzir falso positivo (dado que por acaso casa o padrao), so semeia destinos chamados de
// >= 2 lugares, ou de 1 lugar quando o destino comeca com "push {rets,...}" (0x047N).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import java.util.HashMap;
import java.util.Map;

public class SeedCalls extends GhidraScript {
    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        Map<Long, Integer> hits = new HashMap<>();
        for (String name : new String[] {"flash_app", "sram_code"}) {
            MemoryBlock b = mem.getBlock(name);
            byte[] buf = new byte[(int) b.getSize()];
            b.getBytes(b.getStart(), buf);
            long base = b.getStart().getOffset();
            for (int i = 0; i + 3 < buf.length; i += 2) {
                int b0 = buf[i] & 0xff, b1 = buf[i + 1] & 0xff;
                if (b1 != 0xea || b0 < 0x80 || b0 > 0xbf) continue;
                int disp = ((b0 & 0x3f) << 16) | ((buf[i + 3] & 0xff) << 8) | (buf[i + 2] & 0xff);
                if ((disp & 0x200000) != 0) disp -= 0x400000;
                long target = base + i + 4 + 2L * disp;
                if (inCode(mem, target)) hits.merge(target, 1, Integer::sum);
            }
        }
        int seeded = 0;
        for (Map.Entry<Long, Integer> e : hits.entrySet()) {
            Address t = toAddr(e.getKey());
            int hw = (mem.getByte(t) & 0xff) | ((mem.getByte(t.add(1)) & 0xff) << 8);
            boolean pushRets = (hw & 0xfff0) == 0x0470;
            if (e.getValue() < 2 && !pushRets) continue;
            if (getFunctionAt(t) != null) continue;
            if (disassemble(t) && createFunction(t, null) != null) seeded++;
        }
        println("destinos de call: " + hits.size() + ", funcoes novas semeadas: " + seeded);
    }

    private boolean inCode(Memory mem, long a) {
        if ((a & 1) != 0) return false;
        MemoryBlock b = mem.getBlock(toAddr(a));
        return b != null && (b.getName().equals("flash_app") || b.getName().equals("sram_code"));
    }
}
