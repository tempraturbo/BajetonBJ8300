// Post-script: para cada texto passado como argumento, acha o endereco da string no programa e
// lista quem a referencia (instrucao, funcao). Tambem resume a cobertura da analise.
// Uso: -postScript FindRefs.java "Zone Or Channel" "CH-NAME" ...
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

public class FindRefs extends GhidraScript {
    @Override
    public void run() throws Exception {
        println(String.format("funcoes=%d instrucoes=%d",
                currentProgram.getFunctionManager().getFunctionCount(),
                currentProgram.getListing().getNumInstructions()));
        for (String text : getScriptArgs()) {
            byte[] pat = (text + "\0").getBytes("ISO-8859-1");
            Address a = currentProgram.getMinAddress();
            int n = 0;
            while ((a = find(a, pat)) != null && n < 4) {
                n++;
                StringBuilder sb = new StringBuilder();
                for (Reference r : getReferencesTo(a)) {
                    Address from = r.getFromAddress();
                    Function f = getFunctionContaining(from);
                    Instruction ins = getInstructionAt(from);
                    sb.append(String.format("%n      <- %s %-9s em %s  [%s]", from, r.getReferenceType(),
                            f == null ? "?" : f.getName() + "@" + f.getEntryPoint(), ins == null ? "" : ins.toString()));
                }
                println(String.format("\"%s\" @ %s refs:%s", text, a, sb.length() == 0 ? " nenhuma" : sb));
                a = a.add(1);
            }
            if (n == 0) {
                println(String.format("\"%s\" nao encontrado", text));
            }
        }
    }
}
