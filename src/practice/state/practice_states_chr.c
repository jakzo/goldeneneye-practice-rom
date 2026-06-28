#include "practice_states_chr.h"
#include "practice_states_utils.h"

void save_chr_record(StateStream *stream, const ChrRecord *chr) {
  write_u8(stream, (u8)chr->accuracyrating);
  write_u8(stream, (u8)chr->speedrating);
  write_u8(stream, (u8)chr->arghrating);
  write_u8(stream, chr->grenadeprob);
  write_f32(stream, chr->visionrange);
  write_f32(stream, chr->hearingscale);
  write_u8(stream, chr->morale);
  write_u8(stream, chr->alertness);

  write_u8(stream, (u8)chr->numarghs);
  write_u8(stream, (u8)chr->numclosearghs);
  write_u8(stream, chr->random);
  write_u16(stream, (u16)chr->padpreset1);
  write_u16(stream, (u16)chr->chrpreset1);
  write_u16(stream, (u16)chr->chrseeshot);
  write_u16(stream, (u16)chr->chrseedie);

  write_u32(stream, chr->lastseetarget60);
  write_bytes(stream, &chr->lastknowntargetpos, sizeof(coord3d));
  write_u32(stream, get_tile_offset((StandTile *)chr->targetTile));
  write_u32(stream, chr->seen_bond_time);
  write_u32(stream, chr->lastheartarget60);
}

void load_chr_record(StateStream *stream, ChrRecord *chr) {
  chr->accuracyrating = (s8)read_u8(stream);
  chr->speedrating = (s8)read_u8(stream);
  chr->arghrating = (s8)read_u8(stream);
  chr->grenadeprob = read_u8(stream);
  chr->visionrange = read_f32(stream);
  chr->hearingscale = read_f32(stream);
  chr->morale = read_u8(stream);
  chr->alertness = read_u8(stream);

  chr->numarghs = (s8)read_u8(stream);
  chr->numclosearghs = (s8)read_u8(stream);
  chr->random = read_u8(stream);
  chr->padpreset1 = (s16)read_u16(stream);
  chr->chrpreset1 = (s16)read_u16(stream);
  chr->chrseeshot = (s16)read_u16(stream);
  chr->chrseedie = (s16)read_u16(stream);

  chr->lastseetarget60 = read_u32(stream);
  read_bytes(stream, &chr->lastknowntargetpos, sizeof(coord3d));
  chr->targetTile = get_tile_by_offset((s32)read_u32(stream));
  chr->seen_bond_time = read_u32(stream);
  chr->lastheartarget60 = read_u32(stream);
}
