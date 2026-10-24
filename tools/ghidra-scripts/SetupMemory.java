// Pre-script: monta o mapa de memoria do app.bin do BJ-8300 (JieLi BR23, pi32v2).
// Visto na rotina de partida em 0x01e00100:
//   - app.bin roda da flash a partir de 0x01e00100;
//   - copia 0xd4a8 bytes de 0x01e6ad0c (app.bin + 0x6ac0c) para a RAM em 0x0;
//     as chamadas usam o apelido 0x02000000, os dados o endereco baixo;
//   - zera a .bss em 0xdd20 por 0x11154 bytes; pilha em 0xdcd0.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

public class SetupMemory extends GhidraScript {
    private static final long FLASH_BASE = 0x01e00100L;
    private static final long SEG2_OFF = 0x6ac0cL;
    private static final long SEG2_LEN = 0xd4a8L;
    private static final long RAM_END = 0x20000L;

    @Override
    public void run() throws Exception {
        Memory mem = currentProgram.getMemory();
        MemoryBlock flash = mem.getBlock(toAddr(FLASH_BASE));
        Address seg2Src = toAddr(FLASH_BASE + SEG2_OFF);

        byte[] seg2 = new byte[(int) SEG2_LEN];
        mem.getBytes(seg2Src, seg2);
        MemoryBlock ram = mem.createInitializedBlock("sram_code", toAddr(0x02000000L),
                new java.io.ByteArrayInputStream(seg2), SEG2_LEN, monitor, false);
        ram.setPermissions(true, true, true);
        MemoryBlock alias = mem.createByteMappedBlock("sram_data", toAddr(0x0L), toAddr(0x02000000L), SEG2_LEN, false);
        alias.setPermissions(true, true, false);
        MemoryBlock bss = mem.createUninitializedBlock("sram_bss", toAddr(SEG2_LEN), RAM_END - SEG2_LEN, false);
        bss.setPermissions(true, true, false);

        flash.setName("flash_app");
        flash.setPermissions(true, false, true);
        addEntryPoint(toAddr(FLASH_BASE));
        createFunction(toAddr(FLASH_BASE), "_start");
        createFunction(toAddr(0x01e53a0cL), "main_entry");
        println("mapa montado: flash 0x01e00100, sram_code 0x02000000+" + Long.toHexString(SEG2_LEN)
                + ", sram_data 0x0 (apelido), bss ate 0x" + Long.toHexString(RAM_END));
    }
}
