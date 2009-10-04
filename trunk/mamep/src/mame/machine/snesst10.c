/***************************************************************************

    snesst10.c

    File to handle emulation of the SNES "Seta ST-010" add-on chip.

    Code based on original work by The Dumper, Matthew Kendora,
    Overload and Feather.
    This implementation is based on byuu's BSNES C++ version

***************************************************************************/

typedef struct
{
	INT16 x1, y1, quadrant, theta, o1;
	UINT8 *ram;
} _snes_st010_t;

static _snes_st010_t snes_st010;

static const INT16 st010_sin_table[256] = {
   0x0000,  0x0324,  0x0648,  0x096a,  0x0c8c,  0x0fab,  0x12c8,  0x15e2,
   0x18f9,  0x1c0b,  0x1f1a,  0x2223,  0x2528,  0x2826,  0x2b1f,  0x2e11,
   0x30fb,  0x33df,  0x36ba,  0x398c,  0x3c56,  0x3f17,  0x41ce,  0x447a,
   0x471c,  0x49b4,  0x4c3f,  0x4ebf,  0x5133,  0x539b,  0x55f5,  0x5842,
   0x5a82,  0x5cb3,  0x5ed7,  0x60eb,  0x62f1,  0x64e8,  0x66cf,  0x68a6,
   0x6a6d,  0x6c23,  0x6dc9,  0x6f5e,  0x70e2,  0x7254,  0x73b5,  0x7504,
   0x7641,  0x776b,  0x7884,  0x7989,  0x7a7c,  0x7b5c,  0x7c29,  0x7ce3,
   0x7d89,  0x7e1d,  0x7e9c,  0x7f09,  0x7f61,  0x7fa6,  0x7fd8,  0x7ff5,
   0x7fff,  0x7ff5,  0x7fd8,  0x7fa6,  0x7f61,  0x7f09,  0x7e9c,  0x7e1d,
   0x7d89,  0x7ce3,  0x7c29,  0x7b5c,  0x7a7c,  0x7989,  0x7884,  0x776b,
   0x7641,  0x7504,  0x73b5,  0x7254,  0x70e2,  0x6f5e,  0x6dc9,  0x6c23,
   0x6a6d,  0x68a6,  0x66cf,  0x64e8,  0x62f1,  0x60eb,  0x5ed7,  0x5cb3,
   0x5a82,  0x5842,  0x55f5,  0x539b,  0x5133,  0x4ebf,  0x4c3f,  0x49b4,
   0x471c,  0x447a,  0x41ce,  0x3f17,  0x3c56,  0x398c,  0x36ba,  0x33df,
   0x30fb,  0x2e11,  0x2b1f,  0x2826,  0x2528,  0x2223,  0x1f1a,  0x1c0b,
   0x18f8,  0x15e2,  0x12c8,  0x0fab,  0x0c8c,  0x096a,  0x0648,  0x0324,
   0x0000, -0x0324, -0x0648, -0x096b, -0x0c8c, -0x0fab, -0x12c8, -0x15e2,
  -0x18f9, -0x1c0b, -0x1f1a, -0x2223, -0x2528, -0x2826, -0x2b1f, -0x2e11,
  -0x30fb, -0x33df, -0x36ba, -0x398d, -0x3c56, -0x3f17, -0x41ce, -0x447a,
  -0x471c, -0x49b4, -0x4c3f, -0x4ebf, -0x5133, -0x539b, -0x55f5, -0x5842,
  -0x5a82, -0x5cb3, -0x5ed7, -0x60ec, -0x62f1, -0x64e8, -0x66cf, -0x68a6,
  -0x6a6d, -0x6c23, -0x6dc9, -0x6f5e, -0x70e2, -0x7254, -0x73b5, -0x7504,
  -0x7641, -0x776b, -0x7884, -0x7989, -0x7a7c, -0x7b5c, -0x7c29, -0x7ce3,
  -0x7d89, -0x7e1d, -0x7e9c, -0x7f09, -0x7f61, -0x7fa6, -0x7fd8, -0x7ff5,
  -0x7fff, -0x7ff5, -0x7fd8, -0x7fa6, -0x7f61, -0x7f09, -0x7e9c, -0x7e1d,
  -0x7d89, -0x7ce3, -0x7c29, -0x7b5c, -0x7a7c, -0x7989, -0x7883, -0x776b,
  -0x7641, -0x7504, -0x73b5, -0x7254, -0x70e2, -0x6f5e, -0x6dc9, -0x6c23,
  -0x6a6d, -0x68a6, -0x66cf, -0x64e8, -0x62f1, -0x60eb, -0x5ed7, -0x5cb3,
  -0x5a82, -0x5842, -0x55f5, -0x539a, -0x5133, -0x4ebf, -0x4c3f, -0x49b3,
  -0x471c, -0x447a, -0x41cd, -0x3f17, -0x3c56, -0x398c, -0x36b9, -0x33de,
  -0x30fb, -0x2e10, -0x2b1f, -0x2826, -0x2527, -0x2223, -0x1f19, -0x1c0b,
  -0x18f8, -0x15e2, -0x12c8, -0x0fab, -0x0c8b, -0x096a, -0x0647, -0x0324
};

static const INT16 st010_mode7_scale[176] = {
  0x0380,  0x0325,  0x02da,  0x029c,  0x0268,  0x023b,  0x0215,  0x01f3,
  0x01d5,  0x01bb,  0x01a3,  0x018e,  0x017b,  0x016a,  0x015a,  0x014b,
  0x013e,  0x0132,  0x0126,  0x011c,  0x0112,  0x0109,  0x0100,  0x00f8,
  0x00f0,  0x00e9,  0x00e3,  0x00dc,  0x00d6,  0x00d1,  0x00cb,  0x00c6,
  0x00c1,  0x00bd,  0x00b8,  0x00b4,  0x00b0,  0x00ac,  0x00a8,  0x00a5,
  0x00a2,  0x009e,  0x009b,  0x0098,  0x0095,  0x0093,  0x0090,  0x008d,
  0x008b,  0x0088,  0x0086,  0x0084,  0x0082,  0x0080,  0x007e,  0x007c,
  0x007a,  0x0078,  0x0076,  0x0074,  0x0073,  0x0071,  0x006f,  0x006e,
  0x006c,  0x006b,  0x0069,  0x0068,  0x0067,  0x0065,  0x0064,  0x0063,
  0x0062,  0x0060,  0x005f,  0x005e,  0x005d,  0x005c,  0x005b,  0x005a,
  0x0059,  0x0058,  0x0057,  0x0056,  0x0055,  0x0054,  0x0053,  0x0052,
  0x0051,  0x0051,  0x0050,  0x004f,  0x004e,  0x004d,  0x004d,  0x004c,
  0x004b,  0x004b,  0x004a,  0x0049,  0x0048,  0x0048,  0x0047,  0x0047,
  0x0046,  0x0045,  0x0045,  0x0044,  0x0044,  0x0043,  0x0042,  0x0042,
  0x0041,  0x0041,  0x0040,  0x0040,  0x003f,  0x003f,  0x003e,  0x003e,
  0x003d,  0x003d,  0x003c,  0x003c,  0x003b,  0x003b,  0x003a,  0x003a,
  0x003a,  0x0039,  0x0039,  0x0038,  0x0038,  0x0038,  0x0037,  0x0037,
  0x0036,  0x0036,  0x0036,  0x0035,  0x0035,  0x0035,  0x0034,  0x0034,
  0x0034,  0x0033,  0x0033,  0x0033,  0x0032,  0x0032,  0x0032,  0x0031,
  0x0031,  0x0031,  0x0030,  0x0030,  0x0030,  0x0030,  0x002f,  0x002f,
  0x002f,  0x002e,  0x002e,  0x002e,  0x002e,  0x002d,  0x002d,  0x002d,
  0x002d,  0x002c,  0x002c,  0x002c,  0x002c,  0x002b,  0x002b,  0x002b
};

static const UINT8 st010_arctan[32][32] = {
  { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80 },
  { 0x80, 0xa0, 0xad, 0xb3, 0xb6, 0xb8, 0xb9, 0xba, 0xbb, 0xbb, 0xbc, 0xbc, 0xbd, 0xbd, 0xbd, 0xbd,
    0xbd, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0xbf, 0xbf, 0xbf, 0xbf },
  { 0x80, 0x93, 0xa0, 0xa8, 0xad, 0xb0, 0xb3, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xb9, 0xba, 0xba, 0xbb,
    0xbb, 0xbb, 0xbb, 0xbc, 0xbc, 0xbc, 0xbc, 0xbc, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd, 0xbd },
  { 0x80, 0x8d, 0x98, 0xa0, 0xa6, 0xaa, 0xad, 0xb0, 0xb1, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb7, 0xb8,
    0xb8, 0xb9, 0xb9, 0xba, 0xba, 0xba, 0xba, 0xbb, 0xbb, 0xbb, 0xbb, 0xbb, 0xbc, 0xbc, 0xbc, 0xbc },
  { 0x80, 0x8a, 0x93, 0x9a, 0xa0, 0xa5, 0xa8, 0xab, 0xad, 0xaf, 0xb0, 0xb2, 0xb3, 0xb4, 0xb5, 0xb5,
    0xb6, 0xb7, 0xb7, 0xb8, 0xb8, 0xb8, 0xb9, 0xb9, 0xb9, 0xba, 0xba, 0xba, 0xba, 0xba, 0xbb, 0xbb },
  { 0x80, 0x88, 0x90, 0x96, 0x9b, 0xa0, 0xa4, 0xa7, 0xa9, 0xab, 0xad, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3,
    0xb4, 0xb4, 0xb5, 0xb6, 0xb6, 0xb6, 0xb7, 0xb7, 0xb8, 0xb8, 0xb8, 0xb9, 0xb9, 0xb9, 0xb9, 0xb9 },
  { 0x80, 0x87, 0x8d, 0x93, 0x98, 0x9c, 0xa0, 0xa3, 0xa6, 0xa8, 0xaa, 0xac, 0xad, 0xae, 0xb0, 0xb0,
    0xb1, 0xb2, 0xb3, 0xb4, 0xb4, 0xb5, 0xb5, 0xb6, 0xb6, 0xb6, 0xb7, 0xb7, 0xb7, 0xb8, 0xb8, 0xb8 },
  { 0x80, 0x86, 0x8b, 0x90, 0x95, 0x99, 0x9d, 0xa0, 0xa3, 0xa5, 0xa7, 0xa9, 0xaa, 0xac, 0xad, 0xae,
    0xaf, 0xb0, 0xb1, 0xb2, 0xb2, 0xb3, 0xb3, 0xb4, 0xb4, 0xb5, 0xb5, 0xb6, 0xb6, 0xb6, 0xb7, 0xb7 },
  { 0x80, 0x85, 0x8a, 0x8f, 0x93, 0x97, 0x9a, 0x9d, 0xa0, 0xa2, 0xa5, 0xa6, 0xa8, 0xaa, 0xab, 0xac,
    0xad, 0xae, 0xaf, 0xb0, 0xb0, 0xb1, 0xb2, 0xb2, 0xb3, 0xb3, 0xb4, 0xb4, 0xb5, 0xb5, 0xb5, 0xb5 },
  { 0x80, 0x85, 0x89, 0x8d, 0x91, 0x95, 0x98, 0x9b, 0x9e, 0xa0, 0xa0, 0xa4, 0xa6, 0xa7, 0xa9, 0xaa,
    0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb0, 0xb1, 0xb1, 0xb2, 0xb2, 0xb3, 0xb3, 0xb4, 0xb4, 0xb4 },
  { 0x80, 0x84, 0x88, 0x8c, 0x90, 0x93, 0x96, 0x99, 0x9b, 0x9e, 0xa0, 0xa2, 0xa4, 0xa5, 0xa7, 0xa8,
    0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xaf, 0xb0, 0xb0, 0xb1, 0xb2, 0xb2, 0xb2, 0xb3, 0xb3 },
  { 0x80, 0x84, 0x87, 0x8b, 0x8e, 0x91, 0x94, 0x97, 0x9a, 0x9c, 0x9e, 0xa0, 0xa2, 0xa3, 0xa5, 0xa6,
    0xa7, 0xa9, 0xaa, 0xab, 0xac, 0xac, 0xad, 0xae, 0xae, 0xaf, 0xb0, 0xb0, 0xb1, 0xb1, 0xb2, 0xb2 },
  { 0x80, 0x83, 0x87, 0x8a, 0x8d, 0x90, 0x93, 0x96, 0x98, 0x9a, 0x9c, 0x9e, 0xa0, 0xa2, 0xa3, 0xa5,
    0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xac, 0xad, 0xae, 0xae, 0xaf, 0xb0, 0xb0, 0xb0, 0xb1 },
  { 0x80, 0x83, 0x86, 0x89, 0x8c, 0x8f, 0x92, 0x94, 0x96, 0x99, 0x9b, 0x9d, 0x9e, 0xa0, 0xa2, 0xa3,
    0xa4, 0xa5, 0xa7, 0xa8, 0xa9, 0xa9, 0xaa, 0xab, 0xac, 0xac, 0xad, 0xae, 0xae, 0xaf, 0xaf, 0xb0 },
  { 0x80, 0x83, 0x86, 0x89, 0x8b, 0x8e, 0x90, 0x93, 0x95, 0x97, 0x99, 0x9b, 0x9d, 0x9e, 0xa0, 0xa1,
    0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xaa, 0xab, 0xac, 0xad, 0xad, 0xae, 0xae, 0xaf },
  { 0x80, 0x83, 0x85, 0x88, 0x8b, 0x8d, 0x90, 0x92, 0x94, 0x96, 0x98, 0x9a, 0x9b, 0x9d, 0x9f, 0xa0,
    0xa1, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa8, 0xa9, 0xaa, 0xab, 0xab, 0xac, 0xad, 0xad, 0xae },
  { 0x80, 0x83, 0x85, 0x88, 0x8a, 0x8c, 0x8f, 0x91, 0x93, 0x95, 0x97, 0x99, 0x9a, 0x9c, 0x9d, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa5, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xaa, 0xab, 0xab, 0xac, 0xad },
  { 0x80, 0x82, 0x85, 0x87, 0x89, 0x8c, 0x8e, 0x90, 0x92, 0x94, 0x96, 0x97, 0x99, 0x9b, 0x9c, 0x9d,
    0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa8, 0xa9, 0xaa, 0xaa, 0xab, 0xac },
  { 0x80, 0x82, 0x85, 0x87, 0x89, 0x8b, 0x8d, 0x8f, 0x91, 0x93, 0x95, 0x96, 0x98, 0x99, 0x9b, 0x9c,
    0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa7, 0xa8, 0xa9, 0xa9, 0xaa, 0xab },
  { 0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8c, 0x8e, 0x90, 0x92, 0x94, 0x95, 0x97, 0x98, 0x9a, 0x9b,
    0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa6, 0xa7, 0xa8, 0xa8, 0xa9, 0xaa },
  { 0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8c, 0x8e, 0x90, 0x91, 0x93, 0x94, 0x96, 0x97, 0x99, 0x9a,
    0x9b, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa5, 0xa6, 0xa7, 0xa7, 0xa8, 0xa9 },
  { 0x80, 0x82, 0x84, 0x86, 0x88, 0x8a, 0x8b, 0x8d, 0x8f, 0x90, 0x92, 0x94, 0x95, 0x97, 0x98, 0x99,
    0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa4, 0xa5, 0xa6, 0xa6, 0xa7, 0xa8 },
  { 0x80, 0x82, 0x84, 0x86, 0x87, 0x89, 0x8b, 0x8d, 0x8e, 0x90, 0x91, 0x93, 0x94, 0x96, 0x97, 0x98,
    0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa3, 0xa4, 0xa5, 0xa6, 0xa6, 0xa7 },
  { 0x80, 0x82, 0x84, 0x85, 0x87, 0x89, 0x8a, 0x8c, 0x8e, 0x8f, 0x91, 0x92, 0x94, 0x95, 0x96, 0x98,
    0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa2, 0xa3, 0xa4, 0xa5, 0xa5, 0xa6 },
  { 0x80, 0x82, 0x83, 0x85, 0x87, 0x88, 0x8a, 0x8c, 0x8d, 0x8f, 0x90, 0x92, 0x93, 0x94, 0x96, 0x97,
    0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa2, 0xa3, 0xa4, 0xa5, 0xa5 },
  { 0x80, 0x82, 0x83, 0x85, 0x86, 0x88, 0x8a, 0x8b, 0x8d, 0x8e, 0x90, 0x91, 0x92, 0x94, 0x95, 0x96,
    0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa2, 0xa3, 0xa4, 0xa4 },
  { 0x80, 0x82, 0x83, 0x85, 0x86, 0x88, 0x89, 0x8b, 0x8c, 0x8e, 0x8f, 0x90, 0x92, 0x93, 0x94, 0x95,
    0x96, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa2, 0xa3, 0xa4 },
  { 0x80, 0x82, 0x83, 0x85, 0x86, 0x87, 0x89, 0x8a, 0x8c, 0x8d, 0x8e, 0x90, 0x91, 0x92, 0x93, 0x95,
    0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9e, 0x9f, 0xa0, 0xa1, 0xa1, 0xa2, 0xa3 },
  { 0x80, 0x81, 0x83, 0x84, 0x86, 0x87, 0x89, 0x8a, 0x8b, 0x8d, 0x8e, 0x8f, 0x90, 0x92, 0x93, 0x94,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9e, 0x9f, 0xa0, 0xa1, 0xa1, 0xa2 },
  { 0x80, 0x81, 0x83, 0x84, 0x86, 0x87, 0x88, 0x8a, 0x8b, 0x8c, 0x8e, 0x8f, 0x90, 0x91, 0x92, 0x93,
    0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0x9f, 0xa0, 0xa1, 0xa1 },
  { 0x80, 0x81, 0x83, 0x84, 0x85, 0x87, 0x88, 0x89, 0x8b, 0x8c, 0x8d, 0x8e, 0x90, 0x91, 0x92, 0x93,
    0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0x9f, 0xa0, 0xa1 },
  { 0x80, 0x81, 0x83, 0x84, 0x85, 0x87, 0x88, 0x89, 0x8a, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91, 0x92,
    0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9c, 0x9d, 0x9e, 0x9f, 0x9f, 0xa0 }
};


static INT16 st010_sin( INT16 theta )
{
	return st010_sin_table[(theta >> 8) & 0xff];
}

static INT16 st010_cos( INT16 theta )
{
	return st010_sin_table[((theta + 0x4000) >> 8) & 0xff];
}

static UINT8 st010_readb( UINT16 address )
{
	return snes_st010.ram[address & 0xfff];
}

static UINT16 st010_readw( UINT16 address )
{
	return (st010_readb(address + 0) <<  0) |
			(st010_readb(address + 1) <<  8);
}

static UINT32 st010_readd( UINT16 address )
{
	return (st010_readb(address + 0) <<  0) |
			(st010_readb(address + 1) <<  8) |
			(st010_readb(address + 2) << 16) |
			(st010_readb(address + 3) << 24);
}

static void st010_writeb( UINT16 address, UINT8 data )
{
	snes_st010.ram[address & 0xfff] = data;
}

static void st010_writew( UINT16 address, UINT16 data )
{
	st010_writeb(address + 0, data >> 0);
	st010_writeb(address + 1, data >> 8);
}

static void st010_writed( UINT16 address, UINT32 data )
{
	st010_writeb(address + 0, data >>  0);
	st010_writeb(address + 1, data >>  8);
	st010_writeb(address + 2, data >> 16);
	st010_writeb(address + 3, data >> 24);
}

//

static void st010_op_01_do_work(INT16 x0, INT16 y0) {
  if((x0 < 0) && (y0 < 0)) {
    snes_st010.x1 = -x0;
    snes_st010.y1 = -y0;
    snes_st010.quadrant = -0x8000;
  } else if(x0 < 0) {
    snes_st010.x1 = y0;
    snes_st010.y1 = -x0;
    snes_st010.quadrant = -0x4000;
  } else if(y0 < 0) {
    snes_st010.x1 = -y0;
    snes_st010.y1 = x0;
    snes_st010.quadrant = 0x4000;
  } else {
    snes_st010.x1 = x0;
    snes_st010.y1 = y0;
    snes_st010.quadrant = 0x0000;
  }

  while((snes_st010.x1 > 0x1f) || (snes_st010.y1 > 0x1f)) {
    if(snes_st010.x1 > 1) { snes_st010.x1 >>= 1; }
    if(snes_st010.y1 > 1) { snes_st010.y1 >>= 1; }
  }

  if(snes_st010.y1 == 0) { snes_st010.quadrant += 0x4000; }

  snes_st010.theta = (st010_arctan[snes_st010.y1][snes_st010.x1] << 8) ^ snes_st010.quadrant;
}

//

static void st010_op_01( void ) {
  INT16 x0 = st010_readw(0x0000);
  INT16 y0 = st010_readw(0x0002);

  st010_op_01_do_work(x0, y0);

  st010_writew(0x0000, snes_st010.x1);
  st010_writew(0x0002, snes_st010.y1);
  st010_writew(0x0004, snes_st010.quadrant);
//st010_writew(0x0006, y0);  //Overload's docs note this write occurs, SNES9x disagrees
  st010_writew(0x0010, snes_st010.theta);
}

static void st010_op_02( void ) {
  INT16 positions = st010_readw(0x0024);
  UINT16 *places  = (UINT16*)(snes_st010.ram + 0x0040);
  UINT16 *drivers = (UINT16*)(snes_st010.ram + 0x0080);

  UINT8 sorted;
  UINT16 temp;
  int i;
  if(positions > 1) {
    do {
      sorted = 1;
      for(i = 0; i < positions - 1; i++) {
        if(places[i] < places[i + 1]) {
          temp = places[i + 1];
          places[i + 1] = places[i];
          places[i] = temp;

          temp = drivers[i + 1];
          drivers[i + 1] = drivers[i];
          drivers[i] = temp;

          sorted = 0;
        }
      }
      positions--;
    } while(!sorted);
  }
}

static void st010_op_03( void ) {
  INT16 x0 = st010_readw(0x0000);
  INT16 y0 = st010_readw(0x0002);
  INT16 multiplier = st010_readw(0x0004);
  INT32 x1, y1;

  x1 = x0 * multiplier << 1;
  y1 = y0 * multiplier << 1;

  st010_writed(0x0010, x1);
  st010_writed(0x0014, y1);
}

static void st010_op_04( void ) {
  INT16 x = st010_readw(0x0000);
  INT16 y = st010_readw(0x0002);
  INT16 square;
  //calculate the vector length of (x,y)
  square = sqrt((double)(y * y + x * x));

  st010_writew(0x0010, square);
}

// same as op_01_do_work, but we are only interested in the angle!
static void st010_op_05_do_work(INT16 x0, INT16 y0) {
  INT16 x1, y1, quadrant;
  if((x0 < 0) && (y0 < 0)) {
    x1 = -x0;
    y1 = -y0;
    quadrant = -0x8000;
  } else if(x0 < 0) {
    x1 = y0;
    y1 = -x0;
    quadrant = -0x4000;
  } else if(y0 < 0) {
    x1 = -y0;
    y1 = x0;
    quadrant = 0x4000;
  } else {
    x1 = x0;
    y1 = y0;
    quadrant = 0x0000;
  }

  while((x1 > 0x1f) || (y1 > 0x1f)) {
    if(x1 > 1) { x1 >>= 1; }
    if(y1 > 1) { y1 >>= 1; }
  }

  if(y1 == 0) { quadrant += 0x4000; }

  snes_st010.o1 = (st010_arctan[y1][x1] << 8) ^ quadrant;
}

static void st010_op_05( void ) {
  INT32 dx, dy;
  UINT16 o1 = 0;
  UINT8 wrap = 0;

  //target (x,y) coordinates
  INT16 ypos_max = st010_readw(0x00c0);
  INT16 xpos_max = st010_readw(0x00c2);

  //current coordinates and direction
  INT32 ypos = st010_readd(0x00c4);
  INT32 xpos = st010_readd(0x00c8);
  UINT16 rot = st010_readw(0x00cc);

  //physics
  UINT16 speed = st010_readw(0x00d4);
  UINT16 accel = st010_readw(0x00d6);
  UINT16 speed_max = st010_readw(0x00d8);
  UINT16 old_speed;

  //special condition acknowledgement
  INT16 system = st010_readw(0x00da);
  INT16 flags = st010_readw(0x00dc);

  //new target coordinates
  INT16 ypos_new = st010_readw(0x00de);
  INT16 xpos_new = st010_readw(0x00e0);

  //mask upper bit
  xpos_new &= 0x7fff;

  //get the current distance
  dx = xpos_max - (xpos >> 16);
  dy = ypos_max - (ypos >> 16);

  //quirk: clear and move in9
  st010_writew(0x00d2, 0xffff);
  st010_writew(0x00da, 0x0000);

  //grab the target angle
  st010_op_05_do_work(dy, dx);
  o1 = (UINT16)snes_st010.o1;

  //check for wrapping
  if(abs(o1 - rot) > 0x8000) {
    o1 += 0x8000;
    rot += 0x8000;
    wrap = 1;
  }

  old_speed = speed;

  //special case
  if(abs(o1 - rot) == 0x8000) {
    speed = 0x100;
  }

  //slow down for sharp curves
  else if(abs(o1 - rot) >= 0x1000) {
  UINT32 slow = abs(o1 - rot);
    slow >>= 4;  //scaling
    speed -= slow;
  }

  //otherwise accelerate
  else {
    speed += accel;
    if(speed > speed_max) {
      speed = speed_max;  //clip speed
    }
  }

  //prevent negative/positive overflow
  if(abs(old_speed - speed) > 0x8000) {
    if(old_speed < speed) { speed = 0; }
    else speed = 0xff00;
  }

  //adjust direction by so many degrees
  //be careful of negative adjustments
  if((o1 > rot && (o1 - rot) > 0x80) || (o1 < rot && (rot - o1) >= 0x80)) {
    if(o1 < rot) { rot -= 0x280; }
    else if(o1 > rot) { rot += 0x280; }
  }

  //turn off wrapping
  if(wrap) { rot -= 0x8000; }

  //now check the distances (store for later)
  dx = (xpos_max << 16) - xpos;
  dy = (ypos_max << 16) - ypos;
  dx >>= 16;
  dy >>= 16;

  //if we're in so many units of the target, signal it
  if((system && (dy <= 6 && dy >= -8) && (dx <= 126 && dx >= -128)) || (!system && (dx <= 6 && dx >= -8) && (dy <= 126 && dy >= -128))) {
    //announce our new destination and flag it
    xpos_max = xpos_new & 0x7fff;
    ypos_max = ypos_new;
    flags |= 0x08;
  }

  //update position
  xpos -= (st010_cos(rot) * 0x400 >> 15) * (speed >> 8) << 1;
  ypos -= (st010_sin(rot) * 0x400 >> 15) * (speed >> 8) << 1;

  //quirk: mask upper byte
  xpos &= 0x1fffffff;
  ypos &= 0x1fffffff;

  st010_writew(0x00c0, ypos_max);
  st010_writew(0x00c2, xpos_max);
  st010_writed(0x00c4, ypos);
  st010_writed(0x00c8, xpos);
  st010_writew(0x00cc, rot);
  st010_writew(0x00d4, speed);
  st010_writew(0x00dc, flags);
}

static void st010_op_06( void ) {
  INT16 multiplicand = st010_readw(0x0000);
  INT16 multiplier = st010_readw(0x0002);
  INT32 product;

  product = multiplicand * multiplier << 1;

  st010_writed(0x0010, product);
}

static void st010_op_07( void ) {
  INT16 theta = st010_readw(0x0000);

  INT16 data;
  int i, offset;
  for(i = 0, offset = 0; i < 176; i++) {
    data = st010_mode7_scale[i] * st010_cos(theta) >> 15;
    st010_writew(0x00f0 + offset, data);
    st010_writew(0x0510 + offset, data);

    data = st010_mode7_scale[i] * st010_sin(theta) >> 15;
    st010_writew(0x0250 + offset, data);
    if(data) { data = ~data; }
    st010_writew(0x03b0 + offset, data);

    offset += 2;
  }
}

static void st010_op_08( void ) {
  INT16 x0 = st010_readw(0x0000);
  INT16 y0 = st010_readw(0x0002);
  INT16 theta = st010_readw(0x0004);
  INT16 x1, y1;

  x1 = (y0 * st010_sin(theta) >> 15) + (x0 * st010_cos(theta) >> 15);
  y1 = (y0 * st010_cos(theta) >> 15) - (x0 * st010_sin(theta) >> 15);

  st010_writew(0x0010, x1);
  st010_writew(0x0012, y1);
}

// init, reset & handlers

static UINT8 st010_read( UINT16 address )
{
	return st010_readb(address);
}

static void st010_write( UINT16 address, UINT8 data )
{
	st010_writeb(address, data);

	if ((address & 0xfff) == 0x0021 && (data & 0x80))
	{
		switch (snes_st010.ram[0x0020])
		{
			case 0x01: st010_op_01(); break;
			case 0x02: st010_op_02(); break;
			case 0x03: st010_op_03(); break;
			case 0x04: st010_op_04(); break;
			case 0x05: st010_op_05(); break;
			case 0x06: st010_op_06(); break;
			case 0x07: st010_op_07(); break;
			case 0x08: st010_op_08(); break;
		}

		snes_st010.ram[0x0021] &= ~0x80;
	}
}


static void st010_init( running_machine* machine )
{
	snes_st010.ram = (UINT8*)auto_alloc_array(machine, UINT8, 0x1000);
}

static void st010_reset( void )
{
	snes_st010.x1 = 0;
	snes_st010.y1 = 0;
	snes_st010.quadrant = 0;
	snes_st010.theta = 0;
	memset(snes_st010.ram, 0, 0x1000);
}
