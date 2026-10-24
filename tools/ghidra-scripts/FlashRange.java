// Post-script: descobre ate que endereco de flash o firmware realmente referencia (codigo e dados).
// Serve para saber se o espaco livre depois do app.bin, no fim da imagem gravada, e enderecavel
// pela CPU (execucao direta da flash) ou se fica fora da janela mapeada.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import java.util.TreeMap;

public class FlashRange extends GhidraScript {
    private static final long FLASH_LO = 0x01E00000L, FLASH_HI = 0x01F00000L;

    @Override
    public void run() throws Exception {
        long appEnd = 0x01E00100L + 0x780dcL;
        TreeMap<Long, Address> above = new TreeMap<>();
        long max = 0;
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction ins = it.next();
            for (int i = 0; i < ins.getNumOperands(); i++) {
                for (Object o : ins.getOpObjects(i)) {
                    long v = -1;
                    if (o instanceof Scalar) v = ((Scalar) o).getUnsignedValue();
                    else if (o instanceof Address) v = ((Address) o).getOffset();
                    if (v < FLASH_LO || v >= FLASH_HI) continue;
                    if (v > max) max = v;
                    if (v >= appEnd) above.put(v, ins.getAddress());
                }
            }
        }
        println(String.format("fim do app.bin: 0x%x   maior endereco de flash referenciado: 0x%x", appEnd, max));
        println("referencias acima do fim do app.bin: " + above.size());
        int n = 0;
        for (var e : above.entrySet()) {
            if (n++ >= 20) break;
            println(String.format("   0x%08x  referenciado em %s", e.getKey(), e.getValue()));
        }
    }
}
