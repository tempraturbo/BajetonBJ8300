// Post-script: para cada alvo (um por linha no arquivo de argumento), acha quem o usa e sobe a cadeia
// de chamadores ate a raiz (funcoes sem chamador conhecido, como _start, tarefas e ISRs).
// Linha do arquivo:  0xENDERECO [rotulo]      ou      "texto da string" [rotulo]
// Usuarios de um endereco: referencias do Ghidra + operandos escalares iguais ao endereco + par
// "metade baixa de 16 bits" seguida, em ate 4 instrucoes, da metade alta.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.*;

public class Trace extends GhidraScript {
    private static final int MAX_DEPTH = 12;

    @Override
    public void run() throws Exception {
        println(String.format("funcoes=%d instrucoes=%d", currentProgram.getFunctionManager().getFunctionCount(),
                currentProgram.getListing().getNumInstructions()));
        for (String line : Files.readAllLines(Paths.get(getScriptArgs()[0]), StandardCharsets.UTF_8)) {
            line = line.strip();
            if (line.isEmpty() || line.startsWith("#")) continue;
            Address target;
            String label;
            if (line.startsWith("\"")) {
                int end = line.indexOf('"', 1);
                String text = line.substring(1, end);
                label = line.substring(end + 1).strip();
                target = find(currentProgram.getMinAddress(), (text + "\0").getBytes(StandardCharsets.ISO_8859_1));
                if (target == null) { println("\"" + text + "\" nao encontrado"); continue; }
                label = "\"" + text + "\" " + label;
            } else {
                String[] p = line.split("\\s+", 2);
                target = toAddr(p[0]);
                label = p.length > 1 ? p[1] : "";
            }
            report(target, label);
        }
    }

    private void report(Address target, String label) throws Exception {
        StringBuilder out = new StringBuilder(String.format("##### %s %s", target, label));
        Set<Function> users = new LinkedHashSet<>();
        Function self = getFunctionAt(target);
        if (self != null) {
            users.add(self);
        } else {
            for (Address from : usersOf(target.getOffset())) {
                Function f = getFunctionContaining(from);
                out.append(String.format("%n   usado em %s (%s)", from, f == null ? "fora de funcao" : f.getName()));
                if (f != null) users.add(f);
            }
        }
        for (Function f : users) {
            out.append(String.format("%n   cadeias ate a raiz a partir de %s:", f.getName()));
            List<List<Function>> paths = new ArrayList<>();
            walk(f, new ArrayList<>(), paths);
            int shown = 0;
            for (List<Function> path : paths) {
                if (shown++ >= 8) { out.append(String.format("%n      ... (%d cadeias no total)", paths.size())); break; }
                StringBuilder sb = new StringBuilder();
                for (int i = path.size() - 1; i >= 0; i--) {
                    sb.append(path.get(i).getName()).append(i > 0 ? " -> " : "");
                }
                out.append("\n      ").append(sb);
            }
        }
        println(out.toString());
    }

    private void walk(Function f, List<Function> path, List<List<Function>> paths) {
        if (paths.size() > 200) return;
        path.add(f);
        Set<Function> callers = f.getCallingFunctions(monitor);
        if (callers.isEmpty() || path.size() >= MAX_DEPTH) {
            paths.add(new ArrayList<>(path));
        } else {
            for (Function c : callers) {
                if (path.contains(c)) continue;
                walk(c, path, paths);
            }
        }
        path.remove(path.size() - 1);
    }

    private List<Address> usersOf(long value) {
        List<Address> out = new ArrayList<>();
        for (Reference r : getReferencesTo(toAddr(value))) out.add(r.getFromAddress());
        long lo = value & 0xffff, hi = (value >>> 16) & 0xffff;
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            if (hasScalar(ins, value)) { out.add(ins.getAddress()); continue; }
            if (hasScalar(ins, lo)) {
                Instruction n = ins.getNext();
                for (int k = 0; k < 4 && n != null; k++, n = n.getNext()) {
                    if (hasScalar(n, hi) || hasScalar(n, hi << 16)) { out.add(ins.getAddress()); break; }
                }
            }
        }
        return out;
    }

    private boolean hasScalar(Instruction ins, long v) {
        for (int i = 0; i < ins.getNumOperands(); i++) {
            for (Object o : ins.getOpObjects(i)) {
                if (o instanceof Scalar && ((Scalar) o).getUnsignedValue() == v) return true;
                if (o instanceof Address && ((Address) o).getOffset() == v) return true;
            }
        }
        return false;
    }
}
