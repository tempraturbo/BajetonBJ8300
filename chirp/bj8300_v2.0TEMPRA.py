# CHIRP driver for the Baojie/Bajeton BJ-8300 running the TEMPRA firmware (CHANNEL BANKS).
#
# Derived from chirp/drivers/radtel_rt900.py (Copyright 2025, 2026 Fred Trimble; 2024 Pavel
# Moravec, OK2MOP; 2023 Jim Unroe), which already handles the BJ-8300 as part of the RT-920/BJ7800
# family: channels, settings and the cable protocol all come from it, without copying.
#
# This program is free software: you can redistribute it and/or modify it under the terms of the
# GNU General Public License as published by the Free Software Foundation, either version 2 of
# the License, or (at your option) any later version. It is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY.
#
# ---------------------------------------------------------------------------------------------
# WHAT THIS MODULE ADDS
#
# The modified BJ-8300 firmware (BJ8300_BANK, v1.0T on) keeps 16 channel banks in the data chip,
# OUTSIDE the 64 KB the cable protocol reaches with a 16-bit address:
#
#   0xC1000            header: "BNK3" magic, mask of enabled banks, selected bank, checksum
#   0xC2000 + n*0x1000 bank n: "BCO1" magic, 12-character name, 120-byte map (960 channels at one
#                      bit each) and checksum - one 4 KB sector per bank
#
# To get there the firmware understands a new command that fits in the same 4 bytes as the stock
# one: **bit 7 of the size** set means "24-bit address", and the other 7 bits are the third byte
# of the address; the block is always 64 bytes. With bit 7 clear nothing changes - which is why
# the stock driver keeps working on this radio.
#
# The firmware only accepts the extended write inside 0xC1000..0xD1FFF (header + the 16 banks;
# from v46 on, up to 0xD2FFF, with the WATCH record) and only erases a sector that is empty or
# already its own: no channel, calibration or boot image address is at risk through this path.
#
#   0xD2000            WATCH (firmware v43 on): "WTC1" magic, how many frequencies the main
#                      screen shows (SINGLE, DUAL, TRIPLE), MDF-C, C's channel, whether C is on
#                      side B right now and B's channel meanwhile, checksum - 14 bytes
#
# In the CHIRP image (.img) the banks area goes right after the stock part, at 0xF250, as 17
# sectors of 192 bytes - the same three 64-byte blocks that travel over the cable - and the WATCH
# block (64 bytes) right after it. That way they survive saving and reopening the .img, like any
# other radio data.
# ---------------------------------------------------------------------------------------------

import logging
import struct

from chirp import chirp_common, directory, errors, memmap
from chirp.drivers import radtel_rt900
from chirp.settings import MemSetting, RadioSetting, RadioSettingGroup, RadioSettingSubGroup, \
    RadioSettingValueBoolean, RadioSettingValueInteger, RadioSettingValueList, RadioSettingValueString

LOG = logging.getLogger(__name__)

# --- banks area in the data chip (the same values as the firmware's src/bancos.c) ---
HEADER_SECTOR = 0x0C1000
BANK_SECTOR = 0x0C2000
NUM_BANKS = 16
EXT_BLOCK = 0x40                               # the cable only moves 64-byte blocks
BLOCKS_PER_SECTOR = 3                          # 192 bytes cover the 138-byte record
SECTOR_SIZE = EXT_BLOCK * BLOCKS_PER_SECTOR
BLOB_SIZE = (NUM_BANKS + 1) * SECTOR_SIZE      # header + 16 banks = 3264 bytes

NAME_LEN = 12
MAP_LEN = 120                                  # 960 channels, one bit each
HEADER_LEN = 10
RECORD_LEN = 4 + NAME_LEN + MAP_LEN + 2        # 138
HEADER_MAGIC = b'BNK3'
BANK_MAGIC = b'BCO1'

BLOB_BASE = 0xF250                             # where the banks area goes in the .img

# --- WATCH (the same values as struct vigia in bancos.c) ---
WATCH_SECTOR = 0x0D2000
WATCH_MAGIC = b'WTC1'
WATCH_LEN = 14                                 # magic, mode, MDF-C, C channel, C on B, -, B channel, sum
WATCH_BASE = BLOB_BASE + BLOB_SIZE             # one 64-byte block in the .img, after the banks
IMAGE_SIZE = WATCH_BASE + EXT_BLOCK
WATCH_OPTIONS = ['SINGLE', 'DUAL', 'TRIPLE']   # the radio's menu order
WATCH_VALUES = [1, 0, 2]                       # list position -> value in the record (0 DUAL)
WATCH_SINGLE = 1
NUM_CHANNELS = 960
CHANNEL_SIZE = 32                              # channel records start at 0x0000 of the image
ACK = b'\x06'


def _checksum(data):
    """The firmware's checksum (soma_de in bancos.c): starts at 0x1234 and shifts on every byte."""
    total = 0x1234
    for b in data:
        total = ((total << 1) + b) & 0xFFFF
    return total


def _command(letter, address):
    """Extended packet: 4 bytes, with bit 7 of the size turning on the 24-bit address."""
    return struct.pack('>cHB', letter, address & 0xFFFF, 0x80 | (address >> 16))


def _sector_addresses():
    """Radio address of each of the 17 sectors: the header and the 16 banks."""
    return [HEADER_SECTOR] + [BANK_SECTOR + b * 0x1000 for b in range(NUM_BANKS)]


class _BankStore(object):
    """Reads and writes the banks area inside the image, keeping magic and checksum like the firmware.

    An invalid record (wrong magic or a checksum that does not match) is treated as an empty bank,
    never as garbage: it is the firmware's own rule, so both sides see the same thing.
    """

    def __init__(self, radio):
        self._radio = radio

    # ---- raw access to the image ----
    def _read(self, offset, size):
        return bytes(self._radio._mmap[BLOB_BASE + offset:
                                       BLOB_BASE + offset + size])

    def _write(self, offset, data):
        self._radio._mmap[BLOB_BASE + offset] = bytes(data)

    @staticmethod
    def _offset(bank):
        """Offset of the bank's sector inside the blob (the header takes the first one)."""
        return (bank + 1) * SECTOR_SIZE

    # ---- header ----
    def header(self):
        d = self._read(0, HEADER_LEN)
        if d[:4] == HEADER_MAGIC and _checksum(d[:8]) == struct.unpack('<H', d[8:10])[0]:
            return bytearray(d)
        return bytearray(HEADER_MAGIC + b'\x00' * (HEADER_LEN - 4))

    def write_header(self, h):
        h[8:10] = struct.pack('<H', _checksum(bytes(h[:8])))
        self._write(0, bytes(h) + b'\xff' * (SECTOR_SIZE - HEADER_LEN))

    def enabled_mask(self):
        return struct.unpack('<H', bytes(self.header()[4:6]))[0]

    def set_enabled(self, bank, enabled):
        h = self.header()
        mask = struct.unpack('<H', bytes(h[4:6]))[0]
        bit = 1 << bank
        h[4:6] = struct.pack('<H', mask | bit if enabled else mask & ~bit)
        self.write_header(h)

    def selected(self):
        return self.header()[6]

    def select(self, bank):
        h = self.header()
        h[6] = bank
        self.write_header(h)

    # ---- bank records ----
    def record(self, bank):
        d = self._read(self._offset(bank), RECORD_LEN)
        if d[:4] == BANK_MAGIC and _checksum(d[:136]) == struct.unpack('<H', d[136:])[0]:
            return bytearray(d)
        return bytearray(BANK_MAGIC + b'\x00' * (RECORD_LEN - 4))

    def write_record(self, bank, rec):
        rec[136:138] = struct.pack('<H', _checksum(bytes(rec[:136])))
        # the rest of the sector stays erased, the way the firmware leaves it
        self._write(self._offset(bank), bytes(rec) + b'\xff' * (SECTOR_SIZE - RECORD_LEN))

    def name(self, bank):
        name = bytes(self.record(bank)[4:4 + NAME_LEN])
        return name.split(b'\x00')[0].decode('ascii', 'ignore').rstrip()

    def set_name(self, bank, name):
        rec = self.record(bank)
        rec[4:4 + NAME_LEN] = name.encode('ascii', 'ignore')[:NAME_LEN].ljust(NAME_LEN, b'\x00')
        self.write_record(bank, rec)

    def has(self, bank, channel):
        return bool(self.record(bank)[16 + (channel >> 3)] >> (channel & 7) & 1)

    def put(self, bank, channel, inside):
        rec = self.record(bank)
        bit = 1 << (channel & 7)
        if inside:
            rec[16 + (channel >> 3)] |= bit
        else:
            rec[16 + (channel >> 3)] &= ~bit & 0xFF
        self.write_record(bank, rec)

    def channels(self, bank):
        bank_map = self.record(bank)[16:16 + MAP_LEN]
        return [c for c in range(NUM_CHANNELS) if bank_map[c >> 3] >> (c & 7) & 1]

    # ---- programmed channels and the "enabled" rule ----
    def programmed(self, channel):
        """Whether the channel slot holds a channel, with the stock driver's rule (get_memory in
        radtel_rt900.py): empty when the first byte is 0xFF or the RX frequency is zero."""
        rx = bytes(self._radio._mmap[channel * CHANNEL_SIZE:channel * CHANNEL_SIZE + 4])
        return rx[0] != 0xFF and rx != b'\x00\x00\x00\x00'

    def programmed_count(self, bank):
        return sum(1 for c in self.channels(bank) if self.programmed(c))

    def disable_empty(self):
        """Turns off every enabled bank that has no programmed channel. The radio refuses those
        too (turning one on from its menu is undone right away), so the image never keeps one."""
        h = self.header()
        mask = struct.unpack('<H', bytes(h[4:6]))[0]
        keep = mask
        for bank in range(NUM_BANKS):
            if keep >> bank & 1 and not self.programmed_count(bank):
                keep &= ~(1 << bank)
        if keep != mask:
            h[4:6] = struct.pack('<H', keep)
            self.write_header(h)

    # ---- the whole image, the way it travels over the cable ----
    def sectors(self):
        """The image's 17 sectors, in _sector_addresses() order: the header and the 16 banks."""
        return [self._read(i * SECTOR_SIZE, SECTOR_SIZE) for i in range(NUM_BANKS + 1)]


class _WatchStore(object):
    """The WATCH record inside the image, read and written the firmware's way (le_registro and
    grava_registro in bancos.c): an invalid record reads as DUAL, MDF-C Name and C on channel 1,
    and a value out of range reads as its default (watch_le_tudo).

    Only mode, MDF-C and C's channel are CHIRP's. Whether C is on side B, and B's channel while it
    is, belong to the radio: the upload takes them from the radio, not from the image (merged()).
    """

    def __init__(self, radio):
        self._radio = radio

    def block(self):
        return bytes(self._radio._mmap[WATCH_BASE:WATCH_BASE + EXT_BLOCK])

    @staticmethod
    def valid(block):
        return (block[:4] == WATCH_MAGIC and _checksum(block[:WATCH_LEN - 2]) ==
                struct.unpack('<H', block[WATCH_LEN - 2:WATCH_LEN])[0])

    @staticmethod
    def _record(block):
        if _WatchStore.valid(block):
            return bytearray(block[:WATCH_LEN])
        return bytearray(WATCH_MAGIC + b'\x00' * (WATCH_LEN - 4))

    @staticmethod
    def _block(rec):
        rec[WATCH_LEN - 2:WATCH_LEN] = struct.pack('<H', _checksum(bytes(rec[:WATCH_LEN - 2])))
        return bytes(rec) + b'\xff' * (EXT_BLOCK - WATCH_LEN)   # the rest of the sector stays erased

    def _set(self, offset, data):
        rec = self._record(self.block())
        rec[offset:offset + len(data)] = data
        self._radio._mmap[WATCH_BASE] = self._block(rec)

    def mode(self):
        m = self._record(self.block())[4]
        return m if m <= 2 else 0

    def mdf_c(self):
        m = self._record(self.block())[5]
        return m if m <= 2 else 0

    def channel_c(self):
        c = struct.unpack('<H', self._record(self.block())[6:8])[0]
        return c if c < NUM_CHANNELS else 0

    def c_on_side_b(self):
        return self._record(self.block())[8] != 0

    def set_mode(self, mode):
        self._set(4, bytes([mode]))

    def set_mdf_c(self, mdf):
        self._set(5, bytes([mdf]))

    def set_channel_c(self, channel):
        self._set(6, struct.pack('<H', channel))

    def merged(self, in_radio):
        """The block to upload: this image's mode, MDF-C and C channel over the radio's current
        record (C on side B and B's channel). None if the image has no record of its own."""
        if not self.valid(self.block()):
            return None
        mine = self._record(self.block())
        rec = self._record(in_radio)
        rec[4:8] = mine[4:8]
        return self._block(rec)


class BJ8300Bank(chirp_common.NamedBank):
    """One of the 16 banks: the name lives in the bank's own sector, in the radio."""

    def get_name(self):
        name = self._model._store.name(self._index - 1)
        return name or 'BANK %i' % self._index

    def set_name(self, name):
        self._model._store.set_name(self._index - 1, name)


class BJ8300BankModel(chirp_common.BankModel):
    """16 real banks: a channel can be in as many banks as you like at the same time.

    (CHIRP's base class is meant for "zero or one bank per channel", but the bank editor builds
    one checkbox column per bank and accepts the whole list get_memory_mappings returns, so
    multiple membership works in the UI.)
    """

    def __init__(self, radio, name='Banks'):
        chirp_common.BankModel.__init__(self, radio, name)
        self._store = _BankStore(radio)
        self._banks = [BJ8300Bank(self, i + 1, 'BANK %i' % (i + 1))
                       for i in range(NUM_BANKS)]

    def get_num_mappings(self):
        return NUM_BANKS

    def get_mappings(self):
        return self._banks

    def add_memory_to_mapping(self, memory, mapping):
        self._store.put(mapping.get_index() - 1, memory.number - 1, True)

    def remove_memory_from_mapping(self, memory, mapping):
        bank = mapping.get_index() - 1
        if not self._store.has(bank, memory.number - 1):
            raise errors.InvalidDataError(
                'Channel %i is not in bank %s' % (memory.number, mapping.get_name()))
        self._store.put(bank, memory.number - 1, False)
        self._store.disable_empty()            # a bank left with no channel turns itself off

    def get_mapping_memories(self, mapping):
        return [self._radio.get_memory(c + 1)
                for c in self._store.channels(mapping.get_index() - 1)]

    def get_memory_mappings(self, memory):
        return [b for b in self._banks
                if self._store.has(b.get_index() - 1, memory.number - 1)]


class _BankCableMixin(object):
    """The part that talks to the radio: 24-bit reads and writes, and the firmware check.

    It is a mixin because it applies the same way to the radio and to its VHF/UHF sub-device.
    """

    def _read_extended(self, address):
        """Reads 64 raw bytes from a 24-bit address.

        The 4th byte of the reply is what tells the firmwares apart: the banks firmware answers
        with 0x40 (the cable hook swaps the size for that before answering), while the stock one
        echoes the size it got - with bit 7 set - and sends 4 + that size (0x01E4BB6E..0x01E4BB78),
        that is, more than 128 bytes. The rest of that reply is discarded, so the cable does not
        lose sync, and no extended write is ever attempted.
        """
        command = _command(b'R', address)
        self.pipe.write(command)
        head = self.pipe.read(4)
        if len(head) != 4 or head[0:3] != command[0:3]:
            raise errors.RadioError('The radio did not answer the read at %06X' % address)
        if head[3] != EXT_BLOCK:
            self.pipe.read(head[3])
            raise errors.RadioError(
                'This radio is not running the banks firmware (the 24-bit address is not '
                'accepted). Flash BJ8300_BANK V2.0T, or use the stock Bajeton BJ7800 or '
                'Radtel RT-920 driver for the channels only.')
        data = self.pipe.read(EXT_BLOCK)
        if len(data) != EXT_BLOCK:
            raise errors.RadioError('The radio did not answer the read at %06X' % address)
        return data

    def _write_extended(self, address, data):
        if len(data) != EXT_BLOCK:
            raise errors.RadioError('%i-byte block (it must be %i)' % (len(data), EXT_BLOCK))
        self.pipe.write(_command(b'W', address) + bytes(data))
        if self.pipe.read(1) != ACK:
            raise errors.RadioError('The radio did not acknowledge the write at %06X' % address)

    def _check_firmware(self):
        """One extended read of the header: it fails on the stock firmware (see _read_extended).

        This check is mandatory before any write: on a stock firmware, an extended 'W' would turn
        into a write over the CHANNELS (0xC1000 truncated gives 0x1000).
        (Until 2026-10-24 it compared this read with the normal read of 0x0000 - but the normal
        one comes back decrypted and the extended one raw, so the two never matched, and the lock
        only held because the stock firmware's long reply threw the cable out of sync.)
        """
        self._read_extended(HEADER_SECTOR)

    def _download_banks(self):
        """Reads the 17 sectors (header + 16 banks) and returns the blob that goes into the .img."""
        blob = b''
        status = chirp_common.Status()
        status.msg = 'Reading banks'
        status.max = NUM_BANKS + 1
        for i, address in enumerate(_sector_addresses()):
            status.cur = i
            self.status_fn(status)
            for block in range(BLOCKS_PER_SECTOR):
                blob += self._read_extended(address + block * EXT_BLOCK)
        return blob

    def _upload_banks(self):
        """Writes to the radio only the sectors that differ - the first block erases the sector."""
        sectors = _BankStore(self).sectors()
        status = chirp_common.Status()
        status.msg = 'Writing banks'
        status.max = NUM_BANKS + 1
        for i, (address, data) in enumerate(zip(_sector_addresses(), sectors)):
            status.cur = i
            self.status_fn(status)
            in_radio = b''.join(self._read_extended(address + b * EXT_BLOCK)
                                for b in range(BLOCKS_PER_SECTOR))
            if in_radio == data:
                continue
            for block in range(BLOCKS_PER_SECTOR):
                self._write_extended(address + block * EXT_BLOCK,
                                     data[block * EXT_BLOCK:(block + 1) * EXT_BLOCK])

    def _upload_watch(self):
        """Writes the WATCH record only when this image has one and it changes the radio's.

        An image with no record of its own (saved before this driver read WATCH) leaves the
        radio's alone. The block is read back: up to v45 the firmware ignores an extended write
        outside the banks (and still acknowledges it), so without this the upload would look fine
        and WATCH would not change.
        """
        in_radio = self._read_extended(WATCH_SECTOR)
        data = _WatchStore(self).merged(in_radio)
        if data is None or data == in_radio:
            return
        self._write_extended(WATCH_SECTOR, data)
        if self._read_extended(WATCH_SECTOR) != data:
            raise errors.RadioError(
                'The radio did not keep WATCH, CH-C and MDF-C: they need the BJ8300_BANK firmware '
                'v46 or later. Channels, settings and banks were written.')


# --- roger: the banks firmware adds BEEP 2 and COIN ---
# The radio keeps the roger in the whole byte at 0x9017: 0 OFF, 1 BEEP, 2 TONE1200, and the new
# firmware uses 3 for BEEP 2 and 4 for COIN. The base class reads only the low 2 bits
# (settings.roger; the 6 above are unused_9017), so here the value has 3 bits: bit 2 is the low
# bit of unused_9017. The radio shows OFF, BEEP 1, BEEP 2, COIN, TONE1200, and CHIRP shows the
# same. The base class list has only 3 names: with value 3 it does not even build the screen, so
# the map below applies to this radio only.
ROGER_OPTIONS = ['OFF', 'BEEP 1', 'BEEP 2', 'COIN', 'TONE1200']
ROGER_VALUES = [0, 1, 3, 4, 2]                            # list position -> value in the radio


def _roger_value(settings):
    """The stored roger value: the base class's 2 bits plus the bit above them."""
    return (int(settings.unused_9017) & 1) << 2 | int(settings.roger)


class _RogerSetting(MemSetting):
    """The roger setting in the radio's order: it stores the value, not the list position."""

    def apply_to_memobj(self, memobj):
        settings, value = memobj.settings, ROGER_VALUES[int(self.value)]
        settings.roger = value & 3
        settings.unused_9017 = int(settings.unused_9017) & ~1 | value >> 2


# SCRAMBLE (firmware v25/v1.0T onwards): OFF and 2700 to 3400 Hz in 100 Hz steps, in ascending
# order, as in the radio's menu. The radio keeps a value in the high nibble of a byte - byte 14
# of the channel, byte 16 of the VFO -: 1 = 3400 Hz (the stock ON, the one other BJ-8300s use),
# 2 = 2900 Hz (the Baofeng UV-17L's mode 3) and 3 to 8 the others (scramble_valor in bancos.c);
# for 9 to 15 the radio inverts at 3400 Hz. In the channel, the base class's scramble field takes
# bits 7-2, so it is value << 2 and the map is enough. In the VFO the format splits the nibble
# into unknown3 (bits 7-5) and scramble (bits 4-2), so the setting is replaced by ours.
SCRAMBLE_OPTIONS = ['Off'] + ['%d Hz' % f for f in range(2700, 3401, 100)]
SCRAMBLE_VALUES = [0, 3, 4, 2, 5, 6, 7, 8, 1]             # list position -> value in the radio
SCRAMBLE_DOC = ('Voice inversion. 3400 Hz is this radio\'s stock one (and the one other BJ-8300s '
                'use); 2900 Hz is the Baofeng UV-17L\'s mode 3.')


class _VfoScrambleSetting(MemSetting):
    """The VFO scramble, written like the radio's menu item does (0x01E3D06C): the high nibble of
    byte 16 becomes the value, and the rest of the byte stays."""

    def apply_to_memobj(self, memobj):
        vfo = getattr(memobj.vfo, self._path.split('.')[1])
        n = SCRAMBLE_VALUES[int(self.value)]
        vfo.unknown3 = n >> 1
        vfo.scramble = vfo.scramble & 3 | (n & 1) << 2


def _replace_setting(group, name, new):
    """Replaces the base class setting `name` with what new(old) returns, in the same tree spot."""
    # the top (RadioSettings) is a list of groups; groups map name -> element
    children = (group.items() if isinstance(group, RadioSettingGroup)
                else [(g.get_name(), g) for g in group])
    for key, element in children:
        # RadioSetting is a subclass of RadioSettingGroup in CHIRP: test the setting first
        if isinstance(element, RadioSetting):
            if key == name:
                group._elements[key] = new(element)    # same name and same position in the list
                return True
        elif isinstance(element, RadioSettingGroup) and _replace_setting(element, name, new):
            return True
    return False


def _remove_setting(group, name):
    """Removes the base class setting `name` from wherever it is in the tree."""
    children = (group.items() if isinstance(group, RadioSettingGroup)
                else [(g.get_name(), g) for g in group])
    for key, element in children:
        if isinstance(element, RadioSetting):
            if key == name:
                del group[element]
                return True
        elif isinstance(element, RadioSettingGroup) and _remove_setting(element, name):
            return True
    return False


def _roger_setting(old, roger):
    position = ROGER_VALUES.index(roger) if roger in ROGER_VALUES else 0
    rs = _RogerSetting('settings.roger', old.get_shortname(),
                       RadioSettingValueList(ROGER_OPTIONS, current_index=position))
    rs.set_doc('End-of-transmission tone. BEEP 2 (four notes) and COIN (a game coin) come from '
               'the banks firmware; the others are the stock ones.')
    return rs


def _vfo_scramble_setting(old, side, vfo):
    nibble = int(vfo.unknown3) << 1 | int(vfo.scramble) >> 2
    position = SCRAMBLE_VALUES.index(nibble if nibble in SCRAMBLE_VALUES else 1)   # 9-15: 3400 Hz
    rs = _VfoScrambleSetting('vfo.%s.scramble' % side, old.get_shortname(),
                             RadioSettingValueList(SCRAMBLE_OPTIONS, current_index=position))
    rs.set_doc(SCRAMBLE_DOC)
    return rs


# Until V2.0T this module was bj8300_v1.0TEMPRA.py, and the model was BJ8300_v1.0TEMPRA. An image
# keeps the vendor and model it was saved with, and CHIRP only opens it with a driver whose model
# matches, directly or through directory.MODEL_COMPAT (get_radio_by_image): this keeps the images
# saved with the old module opening here, without a second model in the radio list.
if isinstance(getattr(directory, 'MODEL_COMPAT', None), dict):
    directory.MODEL_COMPAT[('Baojie', 'BJ8300_v1.0TEMPRA')] = ('Baojie', 'BJ8300_v2.0TEMPRA')


@directory.register
class BJ8300(_BankCableMixin, radtel_rt900.BJ7800):
    """Baojie BJ-8300 running the channel banks firmware."""
    VENDOR = 'Baojie'
    MODEL = 'BJ8300_v2.0TEMPRA'        # its own name: it only works with the modified firmware
    VARIANT = ''

    # the channel scramble with the firmware's options, in menu order (see SCRAMBLE_OPTIONS)
    _scramble_map = [(name, v << 2) for name, v in zip(SCRAMBLE_OPTIONS, SCRAMBLE_VALUES)]

    # Channels, settings, bands and protocol are the BJ7800's (960 channels of 32 bytes).
    #
    # ZONES: the modified firmware removed ZONE and "Zone Or Channel" from the menu - the banks do
    # their job. But zone mode still exists in the firmware (flag at b[0x14BD4], which the cable
    # reads and writes at 0x9028, the format's zone_or_channel: 0 = Zone Mode, 1 = Full Channel),
    # and in zone mode up/down is stuck within 64 channels - without the menu item, the radio
    # would have no way out. So every upload writes Full Channel (sync_out). The BJ7800 base class
    # already shows nothing about zones in the settings, and its bank model (15 fixed zones of 64)
    # is replaced by ours.

    def get_bank_model(self):
        return BJ8300BankModel(self, 'Banks')

    def get_features(self):
        rf = super().get_features()
        rf.has_bank = True
        rf.has_bank_names = True
        return rf

    def get_sub_devices(self):
        return [BJ8300VhfUhf(self._mmap),
                radtel_rt900.RT920FM(self._mmap),
                radtel_rt900.RT920AM(self._mmap),
                radtel_rt900.RT920HF(self._mmap)]

    @classmethod
    def get_prompts(cls):
        rp = super().get_prompts()
        rp.experimental = (
            'Driver for the BJ-8300 running the TEMPRA channel banks firmware (V2.0T).\n'
            '\n'
            'The 16 banks are in the Banks tab: one column per bank, and a channel can be in as\n'
            'many banks as you like. Double-click a bank\'s column header to rename it. Each\n'
            'bank\'s name, channel count and on/off switch are also in Settings > Banks.\n'
            '\n'
            'WATCH, CH-C and MDF-C are in Settings > Watch (they need firmware v46 or later).\n'
            '\n'
            'Keep a copy of your first download before changing anything.')
        rp.pre_upload = (
            'The upload rewrites the radio\'s 16 bank sectors from this image: a bank that is\n'
            'empty here will be empty in the radio. If this image came from the stock RT-920 or\n'
            'BJ7800 driver (which do not read banks), every bank in the radio will be erased.\n'
            '\n'
            'The radio is also set to Full Channel (no zones): the banks replace the zones.\n'
            'Banks with no programmed channels are turned off.\n'
            '\n'
            'After the upload, TURN THE RADIO OFF AND ON: the bank filter is built at power-up.')
        return rp

    def process_mmap(self):
        # An image that does not have the banks area or the WATCH block yet (e.g. saved by the
        # RT-920 driver, or by this one before WATCH) gets the space, erased: that way the bank
        # editor opens empty instead of blowing up, and the upload leaves the radio's WATCH alone.
        if self._mmap is not None and len(self._mmap) < IMAGE_SIZE:
            missing = IMAGE_SIZE - len(self._mmap)
            self._mmap = memmap.MemoryMapBytes(self._mmap.get_packed() + b'\xff' * missing)
        super().process_mmap()

    # ONE SESSION for everything (learned the hard way, 2026-10-24): until then the driver read
    # channels and settings through the stock sync_in, which ends with 'E', and opened a second
    # session for the banks. On the radio that failed with "Radio returned unknown identification
    # string": the firmware answers 'E' with an ACK (0x01E4C0D0 -> 0x01E416C4) that CHIRP does not
    # read, and only closes the session on a loop turn with no bytes arriving (0x01E4BA96) - the
    # second session's PROGRAM arrives before that, lands in state 3 and is dropped, and CHIRP
    # reads the leftover ACK as if it were its answer. Now the session is opened once, and 'E'
    # only goes at the end.
    def sync_in(self):
        try:
            data = radtel_rt900.do_download(self)      # opens the session; stock channels/settings
            self._check_firmware()
            blob = self._download_banks() + self._read_extended(WATCH_SECTOR)
        except errors.RadioError:
            raise
        except Exception:
            LOG.exception('Unexpected error reading the radio')
            raise errors.RadioError('Unexpected error reading the radio')
        finally:
            radtel_rt900._exit_programming_mode(self)
        self._mmap = memmap.MemoryMapBytes(data.get_packed()[:BLOB_BASE] + blob)
        self.process_mmap()

    def _upload_stock(self):
        """Channels and settings: the same blocks the stock do_upload writes, in the open session."""
        status = chirp_common.Status()
        status.msg = 'Uploading to radio'
        status.cur = 0
        status.max = sum(end - start + 1 for start, end in self._ranges)
        for start, end in self._ranges:
            for address in range(start, end, self.BLOCK_SIZE_UP):
                status.cur = address + self.BLOCK_SIZE_UP
                self.status_fn(status)
                radtel_rt900._write_block(self, address, self.BLOCK_SIZE_UP)

    def sync_out(self):
        self._memobj.settings.zone_or_channel = 1  # Full Channel: no zones (see the class top)
        if _WatchStore(self).mode() == WATCH_SINGLE:
            self._memobj.settings.tdr = 0          # the firmware's rule: no dual watch in SINGLE
        _BankStore(self).disable_empty()           # never upload an enabled bank with no channel
        try:
            radtel_rt900._enter_programming_mode(self)
            self._check_firmware()                 # before writing anything
            self._upload_stock()
            self._upload_banks()
            self._upload_watch()
        except errors.RadioError:
            raise
        except Exception:
            LOG.exception('Unexpected error writing the radio')
            raise errors.RadioError('Unexpected error writing the radio')
        finally:
            radtel_rt900._exit_programming_mode(self)

    def get_settings(self):
        # The base class builds the roger with its 3-name list and breaks on value 3 (BEEP 2): it
        # builds with 0, then the value goes back and the setting is replaced by our 5-option one.
        roger = _roger_value(self._memobj.settings)
        self._memobj.settings.roger = 0
        try:
            settings = super().get_settings()
        finally:
            self._memobj.settings.roger = roger & 3
        if not _replace_setting(settings, 'settings_roger', lambda old: _roger_setting(old, roger)):
            LOG.warning('roger setting not found in the base class')
        for side in 'ab':
            vfo = getattr(self._memobj.vfo, side)
            if not _replace_setting(settings, 'vfo_%s_scramble' % side,
                                    lambda old: _vfo_scramble_setting(old, side, vfo)):
                LOG.warning('VFO %s scramble not found in the base class', side.upper())
        # The banks firmware (v30 onwards) plays no voice prompt at all: their space holds code now,
        # and the radio's VOICE menu item is gone, so the setting would do nothing.
        if not _remove_setting(settings, 'settings_voice'):
            LOG.warning('voice prompts setting not found in the base class')
        # The banks firmware (v37 onwards) is English only: the LANGUAGE menu item is gone and the
        # radio sets English at every power-on, whatever the memory holds.
        if not _remove_setting(settings, 'settings_language'):
            LOG.warning('language setting not found in the base class')
        settings.append(self._bank_settings())
        settings.append(self._watch_settings())
        return settings

    def _watch_settings(self):
        """Settings > Watch: WATCH, CH-C and MDF-C, as the radio's menu items. Each one is only
        written when it was changed on this page, like the bank settings."""
        store = _WatchStore(self)
        group = RadioSettingGroup('watch', 'Watch')

        rs = RadioSetting('watch_mode', 'Watch (WATCH)', RadioSettingValueList(
            WATCH_OPTIONS, current_index=WATCH_VALUES.index(store.mode())))
        rs.set_doc('How many frequencies the main screen shows. SINGLE: only the one in focus, full '
                   'screen, and dual watch stays off (the upload turns TDR off). DUAL: A and B, as '
                   'stock. TRIPLE: A, B and a third one, C (the channel below); the A/B key goes '
                   'A, B, C. In DUAL and TRIPLE, TDR on listens to all of them, each for the same '
                   'time, and TDR off only to the one in focus. Needs firmware v46 or later.')
        rs.set_apply_callback(self._apply_watch_mode)
        group.append(rs)

        value = RadioSettingValueInteger(1, NUM_CHANNELS, store.channel_c() + 1)
        if store.c_on_side_b():
            value.set_mutable(False)
            doc = ('C is in focus on the radio right now (TRIPLE, on side B), and the radio keeps '
                   'the channel it ends on there. Change it with the radio\'s arrows, or press A/B '
                   'on the radio to move on to A and download again.')
        else:
            doc = ('The memory channel shown and watched as C in TRIPLE. An empty channel is '
                   'skipped: TRIPLE then works as DUAL.')
        rs = RadioSetting('watch_channel_c', 'C channel (CH-C)', value)
        rs.set_doc(doc)
        rs.set_apply_callback(self._apply_watch_channel_c)
        group.append(rs)

        rs = RadioSetting('watch_mdf_c', 'Memory Display Format C (MDF-C)', RadioSettingValueList(
            radtel_rt900.MDF_LIST, current_index=store.mdf_c()))
        rs.set_doc('How C is shown in TRIPLE, like MDF-A and MDF-B.')
        rs.set_apply_callback(self._apply_watch_mdf_c)
        group.append(rs)
        return group

    def _apply_watch_mode(self, setting):
        if setting.value.changed():
            _WatchStore(self).set_mode(WATCH_VALUES[WATCH_OPTIONS.index(str(setting.value))])

    def _apply_watch_channel_c(self, setting):
        if setting.value.changed() and setting.value.get_mutable():
            _WatchStore(self).set_channel_c(int(setting.value) - 1)

    def _apply_watch_mdf_c(self, setting):
        if setting.value.changed():
            _WatchStore(self).set_mdf_c(radtel_rt900.MDF_LIST.index(str(setting.value)))

    def _bank_settings(self):
        """Settings > Banks: one block per bank (Name, Channels, Enabled) and the selected bank.

        Name and Enabled are only written when they were changed on this page (see the apply
        callbacks), so a rename done meanwhile in the Banks tab is not undone by the value this
        page read when it opened. Enabled is checked live as well: a bank with no programmed
        channel cannot be turned on, and CHIRP puts the box back if it is ticked.
        """
        store = _BankStore(self)
        mask = store.enabled_mask()
        group = RadioSettingGroup('banks', 'Banks')
        for i in range(NUM_BANKS):
            count = store.programmed_count(i)
            block = RadioSettingSubGroup('bank_%i' % (i + 1), 'BANK %i' % (i + 1))

            rs = RadioSetting('bank_%i_name' % (i + 1), 'Name',
                              RadioSettingValueString(0, NAME_LEN, store.name(i), autopad=False))
            rs.set_doc('Bank name, up to %i characters, as the radio shows it in BANK.' % NAME_LEN)
            rs.set_apply_callback(self._apply_bank_name, i)
            block.append(rs)

            value = RadioSettingValueString(0, 4, str(count), autopad=False)
            value.set_mutable(False)
            rs = RadioSetting('bank_%i_channels' % (i + 1), 'Channels', value)
            rs.set_doc('Programmed channels in this bank, counted when this page was opened. '
                       'Add or remove channels in the Banks tab.')
            block.append(rs)

            value = RadioSettingValueBoolean(bool(mask >> i & 1) and count > 0)
            value.set_validate_callback(lambda on, i=i: bool(on) and store.programmed_count(i) > 0)
            rs = RadioSetting('bank_%i_enabled' % (i + 1), 'Enabled', value)
            rs.set_doc('With one or more banks enabled, up/down and scan only see the channels of '
                       'those banks; with none enabled, the radio works as stock. A bank with no '
                       'programmed channel cannot be enabled, and removing its last channel turns '
                       'it off.')
            rs.set_apply_callback(self._apply_bank_enabled, i)
            block.append(rs)
            group.append(block)

        selected = store.selected()
        options = ['BANK %i%s' % (i + 1, ' (%s)' % store.name(i) if store.name(i) else '')
                   for i in range(NUM_BANKS)]
        rs = RadioSetting('bank_selected', 'Bank edited by the radio menu',
                          RadioSettingValueList(
                              options, current_index=selected if selected < NUM_BANKS else 0))
        rs.set_doc('The bank that the radio\'s BANK NAME and BANK CH menu items work on.')
        rs.set_apply_callback(self._apply_bank_selected)
        group.append(rs)
        return group

    def _apply_bank_name(self, setting, bank):
        if setting.value.changed():
            _BankStore(self).set_name(bank, str(setting.value).strip())

    def _apply_bank_enabled(self, setting, bank):
        if setting.value.changed():
            _BankStore(self).set_enabled(bank, bool(setting.value))

    def _apply_bank_selected(self, setting):
        _BankStore(self).select(int(setting.value))   # RadioSettingValueList gives the index


class BJ8300VhfUhf(BJ8300):
    """VHF/UHF sub-device: this is where the 960 channels are and, with them, the banks."""
    VENDOR = 'Baojie'
    MODEL = 'BJ8300_v2.0TEMPRA'
    VARIANT = 'VHF/UHF'
