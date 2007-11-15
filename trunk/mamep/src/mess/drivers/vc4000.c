/******************************************************************************
 Peter.Trauner@jk.uni-linz.ac.at May 2001

 Paul Robson's Emulator at www.classicgaming.com/studio2 made it possible
******************************************************************************/

#include "driver.h"
#include "cpu/s2650/s2650.h"
#include "mslegacy.h"

#include "includes/vc4000.h"
#include "devices/cartslot.h"
#include "image.h"

static  READ8_HANDLER(vc4000_key_r)
{
	UINT8 data=0;
	switch(offset) {
	case 0:
		data = readinputport(1);
		break;
	case 1:
		data = readinputport(2);
		break;
	case 2:
		data = readinputport(3);
		break;
	case 3:
		data = readinputport(0);
		break;
	case 4:
		data = readinputport(4);
		break;
	case 5:
		data = readinputport(5);
		break;
	case 6:
		data = readinputport(6);
		break;
	}
	return data;
}


static ADDRESS_MAP_START( vc4000_mem , ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE( 0x0000, 0x17ff) AM_ROM
	AM_RANGE( 0x1800, 0x1bff) AM_RAM
	AM_RANGE( 0x1e88, 0x1e8e) AM_READ( vc4000_key_r )
	AM_RANGE( 0x1f00, 0x1fff) AM_READWRITE( vc4000_video_r, vc4000_video_w )
ADDRESS_MAP_END

static ADDRESS_MAP_START( vc4000_io , ADDRESS_SPACE_IO, 8)
	AM_RANGE( S2650_SENSE_PORT,S2650_SENSE_PORT) AM_READ( vc4000_vsync_r)
ADDRESS_MAP_END

static INPUT_PORTS_START( vc4000 )
	PORT_START
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Start") PORT_CODE(KEYCODE_F1)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Game Select") PORT_CODE(KEYCODE_F2)
	PORT_START
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 1") PORT_CODE(KEYCODE_1)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 4") PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_Q)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 7") PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_A)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left Enter") PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_Z)
	PORT_START
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 2/Button") PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_LCONTROL)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 5") PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_W)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 8") PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_S)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 0") PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_X)
	PORT_START
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 3") PORT_CODE(KEYCODE_3)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 6") PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_E)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left 9") PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_D)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/Left Clear") PORT_CODE(KEYCODE_C) PORT_CODE(KEYCODE_ESC)
	PORT_START
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 1") PORT_CODE(KEYCODE_1_PAD)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 4") PORT_CODE(KEYCODE_4_PAD)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 7") PORT_CODE(KEYCODE_7_PAD)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right ENTER") PORT_CODE(KEYCODE_ENTER_PAD)
	PORT_START
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 2/Button") PORT_CODE(KEYCODE_2_PAD) PORT_CODE(KEYCODE_LALT)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 5") PORT_CODE(KEYCODE_5_PAD)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 8") PORT_CODE(KEYCODE_8_PAD)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 0") PORT_CODE(KEYCODE_0_PAD)
	PORT_START
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 3") PORT_CODE(KEYCODE_3_PAD)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 6") PORT_CODE(KEYCODE_6_PAD)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right 9") PORT_CODE(KEYCODE_9_PAD)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/Right Clear") PORT_CODE(KEYCODE_DEL_PAD)
#ifndef ANALOG_HACK
    // shit, auto centering too slow, so only using 5 bits, and scaling at videoside
    PORT_START
PORT_BIT(0x1ff,0x70,IPT_AD_STICK_X) PORT_SENSITIVITY(100) PORT_KEYDELTA(1) PORT_MINMAX(20,225) PORT_CODE_DEC(KEYCODE_LEFT) PORT_CODE_INC(KEYCODE_RIGHT) PORT_CODE_DEC(JOYCODE_1_LEFT) PORT_CODE_INC(JOYCODE_1_RIGHT) PORT_RESET
    PORT_START
PORT_BIT(0x1ff,0x70,IPT_AD_STICK_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(1) PORT_MINMAX(20,225) PORT_CODE_DEC(KEYCODE_UP) PORT_CODE_INC(KEYCODE_DOWN) PORT_CODE_DEC(JOYCODE_1_UP) PORT_CODE_INC(JOYCODE_1_DOWN) PORT_RESET
    PORT_START
PORT_BIT(0x1ff,0x70,IPT_AD_STICK_X) PORT_SENSITIVITY(100) PORT_KEYDELTA(1) PORT_MINMAX(20,225) PORT_CODE_DEC(KEYCODE_DEL) PORT_CODE_INC(KEYCODE_PGDN) PORT_CODE_DEC(JOYCODE_2_LEFT) PORT_CODE_INC(JOYCODE_2_RIGHT) PORT_PLAYER(2) PORT_RESET
    PORT_START
PORT_BIT(0x1ff,0x70,IPT_AD_STICK_Y) PORT_SENSITIVITY(100) PORT_KEYDELTA(1) PORT_MINMAX(20,225) PORT_CODE_DEC(KEYCODE_HOME) PORT_CODE_INC(KEYCODE_END) PORT_CODE_DEC(JOYCODE_2_UP) PORT_CODE_INC(JOYCODE_2_DOWN) PORT_PLAYER(2) PORT_RESET
#else
	PORT_START
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/left") PORT_CODE(KEYCODE_LEFT)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/right") PORT_CODE(KEYCODE_RIGHT)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/down") PORT_CODE(KEYCODE_DOWN)
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 1/up") PORT_CODE(KEYCODE_UP)
	PORT_BIT(0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/left") PORT_CODE(KEYCODE_DEL)
	PORT_BIT(0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/right") PORT_CODE(KEYCODE_PGDN)
	PORT_BIT(0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/down") PORT_CODE(KEYCODE_END)
	PORT_BIT(0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("Player 2/up") PORT_CODE(KEYCODE_HOME)
#endif
INPUT_PORTS_END

static const gfx_layout vc4000_charlayout =
{
	8,1,
	256,                                    /* 256 characters */
	1,                      /* 1 bits per pixel */
	{ 0 },                  /* no bitplanes; 1 bit per pixel */
	/* x offsets */
	{
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	},
	/* y offsets */
	{ 0 },
	1*8
};

static GFXDECODE_START( vc4000_gfxdecodeinfo )
	GFXDECODE_ENTRY( REGION_GFX1, 0x0000, vc4000_charlayout, 0, 2 )
GFXDECODE_END

#ifdef UNUSED_FUNCTION
static INTERRUPT_GEN( vc4000 )
{
}
#endif

static const unsigned char vc4000_palette[] =
{
	// background colors
	0, 0, 0, // black
	0, 0, 255, // blue
	0, 255, 0, // green
	0, 255, 255, // cyan
	255, 0, 0, // red
	255, 0, 255, // magenta
	255, 255, 0, // yellow
	255, 255, 255, // white
	// sprite colors
	// simplier to add another 8 colors else using colormapping
	// xor 7, bit 2 not green, bit 1 not blue, bit 0 not red
	255, 255, 255, // white
	255, 255, 0, // yellow
	255, 0, 255, // magenta
	255, 0, 0, // red
	0, 255, 255, // cyan
	0, 255, 0, // green
	0, 0, 255, // blue
	0, 0, 0 // black
};

static const unsigned short vc4000_colortable[1][2] =
{
	{ 0, 1 },
};

static PALETTE_INIT( vc4000 )
{
	palette_set_colors_rgb(machine, 0, vc4000_palette, sizeof(vc4000_palette) / 3);
	memcpy(colortable, vc4000_colortable,sizeof(vc4000_colortable));
}

static MACHINE_DRIVER_START( vc4000 )
	/* basic machine hardware */
	MDRV_CPU_ADD(S2650, 865000)        /* 3550000/4, 3580000/3, 4430000/3 */
	MDRV_CPU_PROGRAM_MAP(vc4000_mem, 0)
	MDRV_CPU_IO_MAP(vc4000_io, 0)
	MDRV_CPU_PERIODIC_INT(vc4000_video_line, 312*50)
	MDRV_SCREEN_REFRESH_RATE(50)
	MDRV_SCREEN_VBLANK_TIME(DEFAULT_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(1)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(226, 312)
	MDRV_SCREEN_VISIBLE_AREA(10, 182, 0, 269)
	MDRV_GFXDECODE( vc4000_gfxdecodeinfo )
	MDRV_PALETTE_LENGTH(sizeof(vc4000_palette) / 3)
	MDRV_COLORTABLE_LENGTH(sizeof (vc4000_colortable) / sizeof(vc4000_colortable[0][0]))
	MDRV_PALETTE_INIT( vc4000 )

	MDRV_VIDEO_START( vc4000 )
	MDRV_VIDEO_UPDATE( vc4000 )

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD(CUSTOM, 0)
	MDRV_SOUND_CONFIG(vc4000_sound_interface)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
MACHINE_DRIVER_END

ROM_START(vc4000)
	ROM_REGION(0x8000,REGION_CPU1, 0)
	ROM_CART_LOAD(0, "bin", 0x0000, 0x8000, ROM_NOMIRROR)

	ROM_REGION(0x100,REGION_GFX1, 0)
ROM_END

SYSTEM_CONFIG_START(vc4000)
	CONFIG_DEVICE(cartslot_device_getinfo)
SYSTEM_CONFIG_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

static DRIVER_INIT( vc4000 )
{
	int i;
	UINT8 *gfx=memory_region(REGION_GFX1);
	for (i=0; i<256; i++) gfx[i]=i;
}

/*    YEAR	NAME	PARENT	COMPAT	MACHINE	INPUT	INIT	CONFIG	COMPANY		FULLNAME */
CONS(1978,	vc4000,	0,		0,		vc4000,	vc4000,	vc4000,	vc4000,	"Interton",	"VC4000", GAME_IMPERFECT_GRAPHICS )
