#ifndef BIOS_H
#define BIOS_H

extern void BIOS_ArcTan();
extern void BIOS_ArcTan2();
extern void BIOS_BitUnPack();
extern void BIOS_GetBiosChecksum();
extern void BIOS_BgAffineSet();
extern void BIOS_CpuSet();
extern void BIOS_CpuFastSet();
extern void BIOS_Diff8bitUnFilterWram();
extern void BIOS_Diff8bitUnFilterVram();
extern void BIOS_Diff16bitUnFilter();
extern void BIOS_Div();
extern void BIOS_DivARM();
extern void BIOS_HuffUnComp();
extern void BIOS_LZ77UnCompVram();
extern void BIOS_LZ77UnCompWram();
extern void BIOS_ObjAffineSet();
extern void BIOS_RegisterRamReset();
extern void BIOS_RegisterRamReset(uint32_t);
extern void BIOS_RLUnCompVram();
extern void BIOS_RLUnCompWram();
extern void BIOS_SoftReset();
extern void BIOS_Sqrt();
extern void BIOS_MidiKey2Freq();
extern void BIOS_SndDriverJmpTableCopy();
extern void BIOS_SndDriverInit();
extern void BIOS_SndDriverMode();
extern void BIOS_SndDriverMain();
extern void BIOS_SndDriverVSync();
extern void BIOS_SndDriverVSyncOff();
extern void BIOS_SndDriverVSyncOn();
extern void BIOS_SndChannelClear();

#endif // BIOS_H
