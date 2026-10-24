// Post-script: exporta "inicio fim nome" de todas as funcoes para o arquivo do argumento, para cruzar
// varreduras feitas fora do Ghidra (ex.: chamadas cruas jaddr23) com os limites das funcoes.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.listing.Function;
import java.io.PrintWriter;

public class ExportFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        try (PrintWriter w = new PrintWriter(getScriptArgs()[0], "UTF-8")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                for (AddressRange r : f.getBody()) {
                    w.printf("%x %x %s%n", r.getMinAddress().getOffset(), r.getMaxAddress().getOffset(), f.getName());
                }
            }
        }
        println("funcoes exportadas: " + currentProgram.getFunctionManager().getFunctionCount());
    }
}
