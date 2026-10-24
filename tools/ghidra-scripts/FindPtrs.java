// Post-script: acha strings e quem aponta para elas por VALOR (4 bytes little-endian), nao so por
// referencia do Ghidra: tabelas de ponteiros e imediatos de "mov rX,#addr" aparecem assim.
// Uso: -postScript FindPtrs.java <arquivo_de_termos>   (um termo por linha; evita o .bat do Ghidra)
// Para cada ocorrencia: se cai numa instrucao, mostra instrucao e funcao; se cai em dado, mostra a
// vizinhanca como palavras de 32 bits, anotando as que apontam para strings ou funcoes.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class FindPtrs extends GhidraScript {
    private Memory mem;

    @Override
    public void run() throws Exception {
        mem = currentProgram.getMemory();
        for (String term : Files.readAllLines(Paths.get(getScriptArgs()[0]), StandardCharsets.UTF_8)) {
            if (term.isBlank()) continue;
            byte[] pat = (term + "\0").getBytes(StandardCharsets.ISO_8859_1);
            Address s = find(currentProgram.getMinAddress(), pat);
            if (s == null) {
                println("\"" + term + "\" nao encontrado");
                continue;
            }
            long v = s.getOffset();
            byte[] le = {(byte) v, (byte) (v >> 8), (byte) (v >> 16), (byte) (v >> 24)};
            StringBuilder out = new StringBuilder(String.format("\"%s\" @ %s", term, s));
            Address a = currentProgram.getMinAddress();
            int n = 0;
            while ((a = find(a, le)) != null && n < 12) {
                n++;
                Instruction ins = getInstructionContaining(a);
                if (ins != null) {
                    Function f = getFunctionContaining(a);
                    out.append(String.format("%n   instr %s  %s  em %s", ins.getAddress(), ins,
                            f == null ? "?" : f.getName()));
                } else {
                    out.append(String.format("%n   dado  %s:%s", a, neighborhood(a)));
                }
                a = a.add(1);
            }
            if (n == 0) out.append("  (nenhum ponteiro com esse valor)");
            println(out.toString());
        }
    }

    private String neighborhood(Address a) throws Exception {
        StringBuilder sb = new StringBuilder();
        for (int i = -6; i <= 6; i++) {
            Address w = a.add(4L * i);
            if (!mem.contains(w) || !mem.contains(w.add(3))) continue;
            long val = mem.getInt(w) & 0xffffffffL;
            sb.append(String.format("%n        %s%s %08x %s", i == 0 ? ">" : " ", w, val, describe(val)));
        }
        return sb.toString();
    }

    private String describe(long val) {
        try {
            Address t = toAddr(val);
            if (!mem.contains(t)) return "";
            Function f = getFunctionAt(t);
            if (f != null) return "-> funcao " + f.getName();
            StringBuilder s = new StringBuilder();
            for (int i = 0; i < 40; i++) {
                int b = mem.getByte(t.add(i)) & 0xff;
                if (b == 0) break;
                if (b < 0x20 || b > 0x7e) return "";
                s.append((char) b);
            }
            return s.length() >= 2 ? "-> \"" + s + "\"" : "";
        } catch (Exception e) {
            return "";
        }
    }
}
