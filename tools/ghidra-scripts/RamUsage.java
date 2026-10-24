// Post-script: mapeia quais enderecos da RAM (0x0..0x20000) o firmware realmente referencia, para
// achar uma faixa livre onde o codigo dos bancos possa guardar o seu estado.
// Varre todos os operandos escalares/enderecos das instrucoes; imprime os maiores vazios.
// Uso: -postScript RamUsage.java [tamanho_minimo_do_vazio_em_bytes]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import java.util.TreeSet;

public class RamUsage extends GhidraScript {
    private static final long RAM_END = 0x20000L;

    @Override
    public void run() throws Exception {
        long minGap = getScriptArgs().length > 0 ? Long.decode(getScriptArgs()[0]) : 256;
        TreeSet<Long> used = new TreeSet<>();
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        int n = 0;
        while (it.hasNext()) {
            Instruction ins = it.next();
            n++;
            for (int i = 0; i < ins.getNumOperands(); i++) {
                for (Object o : ins.getOpObjects(i)) {
                    long v = -1;
                    if (o instanceof Scalar) v = ((Scalar) o).getUnsignedValue();
                    else if (o instanceof Address) v = ((Address) o).getOffset();
                    if (v > 0 && v < RAM_END) used.add(v);
                }
            }
        }
        println(String.format("instrucoes=%d  valores distintos na faixa da RAM=%d", n, used.size()));
        long prev = 0;
        for (long v : used) {
            if (v - prev >= minGap) {
                println(String.format("   vazio 0x%05x..0x%05x  (%d bytes)", prev, v, v - prev));
            }
            prev = v;
        }
        if (RAM_END - prev >= minGap) {
            println(String.format("   vazio 0x%05x..0x%05x  (%d bytes) ate o fim", prev, RAM_END, RAM_END - prev));
        }
        println("maior valor referenciado: 0x" + Long.toHexString(used.last()));
    }
}
