/***************************************************************************

 Amidar hardware

***************************************************************************/

#include "driver.h"
#include "galaxian.h"
#include "sound/ay8910.h"
#include "machine/8255ppi.h"


static const gfx_layout amidar_charlayout =
{
	8,8,
	RGN_FRAC(1,2),
	2,
	{ RGN_FRAC(0,2), RGN_FRAC(1,2) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};

static const gfx_layout amidar_spritelayout =
{
	16,16,
	RGN_FRAC(1,2),
	2,
	{ RGN_FRAC(0,2), RGN_FRAC(1,2) },
	{ 0, 1, 2, 3, 4, 5, 6, 7,
			8*8+0, 8*8+1, 8*8+2, 8*8+3, 8*8+4, 8*8+5, 8*8+6, 8*8+7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8 },
	32*8
};


static GFXDECODE_START( amidar )
	GFXDECODE_ENTRY( REGION_GFX1, 0x0000, amidar_charlayout,   0, 8 )
	GFXDECODE_ENTRY( REGION_GFX1, 0x0000, amidar_spritelayout, 0, 8 )
GFXDECODE_END


static UINT8 *amidar_soundram;

static READ8_HANDLER(amidar_soundram_r)
{
	return amidar_soundram[offset & 0x03ff];
}

static WRITE8_HANDLER(amidar_soundram_w)
{
	amidar_soundram[offset & 0x03ff] = data;
}

static const struct AY8910interface amidar_ay8910_interface_2 =
{
	soundlatch_r,
	scramble_portB_r
};

static READ8_HANDLER(amidar_ppi8255_0_r)
{
	return ppi8255_0_r(machine, offset >> 4);
}

static READ8_HANDLER(amidar_ppi8255_1_r)
{
	return ppi8255_1_r(machine, offset >> 4);
}

static WRITE8_HANDLER(amidar_ppi8255_0_w)
{
	ppi8255_0_w(machine, offset >> 4, data);
}

static WRITE8_HANDLER(amidar_ppi8255_1_w)
{
	ppi8255_1_w(machine, offset >> 4, data);
}


static ADDRESS_MAP_START( readmem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_READ(MRA8_ROM)
	AM_RANGE(0x8000, 0x87ff) AM_READ(MRA8_RAM)
	AM_RANGE(0x9000, 0x93ff) AM_READ(MRA8_RAM)
	AM_RANGE(0x9800, 0x98ff) AM_READ(MRA8_RAM)
	AM_RANGE(0xa800, 0xa800) AM_READ(watchdog_reset_r)
	AM_RANGE(0xb000, 0xb03f) AM_READ(amidar_ppi8255_0_r)
	AM_RANGE(0xb800, 0xb83f) AM_READ(amidar_ppi8255_1_r)
ADDRESS_MAP_END

static ADDRESS_MAP_START( writemem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_WRITE(MWA8_ROM)
	AM_RANGE(0x8000, 0x87ff) AM_WRITE(MWA8_RAM)
	AM_RANGE(0x9000, 0x93ff) AM_WRITE(galaxian_videoram_w) AM_BASE(&galaxian_videoram)
	AM_RANGE(0x9800, 0x983f) AM_WRITE(galaxian_attributesram_w) AM_BASE(&galaxian_attributesram)
	AM_RANGE(0x9840, 0x985f) AM_WRITE(MWA8_RAM) AM_BASE(&galaxian_spriteram) AM_SIZE(&galaxian_spriteram_size)
	AM_RANGE(0x9860, 0x98ff) AM_WRITE(MWA8_RAM)
	AM_RANGE(0xa000, 0xa000) AM_WRITE(scramble_background_red_w)
	AM_RANGE(0xa008, 0xa008) AM_WRITE(galaxian_nmi_enable_w)
	AM_RANGE(0xa010, 0xa010) AM_WRITE(galaxian_flip_screen_x_w)
	AM_RANGE(0xa018, 0xa018) AM_WRITE(galaxian_flip_screen_y_w)
	AM_RANGE(0xa020, 0xa020) AM_WRITE(scramble_background_green_w)
	AM_RANGE(0xa028, 0xa028) AM_WRITE(scramble_background_blue_w)
	AM_RANGE(0xa030, 0xa030) AM_WRITE(galaxian_coin_counter_0_w)
	AM_RANGE(0xa038, 0xa038) AM_WRITE(galaxian_coin_counter_1_w)
	AM_RANGE(0xb000, 0xb03f) AM_WRITE(amidar_ppi8255_0_w)
	AM_RANGE(0xb800, 0xb83f) AM_WRITE(amidar_ppi8255_1_w)
ADDRESS_MAP_END


static ADDRESS_MAP_START( amidar_sound_readmem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x2fff) AM_READ(MRA8_ROM)
	AM_RANGE(0x8000, 0x8fff) AM_READ(amidar_soundram_r)
ADDRESS_MAP_END

static ADDRESS_MAP_START( amidar_sound_writemem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x2fff) AM_WRITE(MWA8_ROM)
	AM_RANGE(0x8000, 0x8fff) AM_WRITE(amidar_soundram_w)
	AM_RANGE(0x8000, 0x83ff) AM_WRITE(MWA8_NOP) AM_BASE(&amidar_soundram)  /* only here to initialize pointer */
	AM_RANGE(0x9000, 0x9fff) AM_WRITE(scramble_filter_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START( amidar_sound_readport, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_FLAGS( AMEF_ABITS(8) )
	AM_RANGE(0x20, 0x20) AM_READ(AY8910_read_port_0_r)
	AM_RANGE(0x80, 0x80) AM_READ(AY8910_read_port_1_r)
ADDRESS_MAP_END

static ADDRESS_MAP_START( amidar_sound_writeport, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_FLAGS( AMEF_ABITS(8) )
	AM_RANGE(0x10, 0x10) AM_WRITE(AY8910_control_port_0_w)
	AM_RANGE(0x20, 0x20) AM_WRITE(AY8910_write_port_0_w)
	AM_RANGE(0x40, 0x40) AM_WRITE(AY8910_control_port_1_w)
	AM_RANGE(0x80, 0x80) AM_WRITE(AY8910_write_port_1_w)
ADDRESS_MAP_END

#define AMIDAR_IN0 \
PORT_START_TAG("IN0") \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY PORT_COCKTAIL\
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably space for button 2 */\
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )\
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )\
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY\
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY\
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )\
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

#define AMIDAR_IN1 \
	PORT_START_TAG("IN1")\
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )\
	PORT_DIPSETTING(    0x03, "3" )\
	PORT_DIPSETTING(    0x02, "4" )\
	PORT_DIPSETTING(    0x01, "5" )\
	PORT_DIPSETTING(    0x00, "255 (Cheat)")\
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably space for player 2 button 2 */\
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL\
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY PORT_COCKTAIL\
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY PORT_COCKTAIL\
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )\
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

#define  AMIDAR_DSW \
	PORT_START_TAG("DSW")\
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coin_A ) )\
	PORT_DIPSETTING(    0x04, DEF_STR( 4C_1C ) )\
	PORT_DIPSETTING(    0x0a, DEF_STR( 3C_1C ) )\
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) )\
	PORT_DIPSETTING(    0x02, DEF_STR( 3C_2C ) )\
	PORT_DIPSETTING(    0x08, DEF_STR( 4C_3C ) )\
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )\
	PORT_DIPSETTING(    0x0c, DEF_STR( 3C_4C ) )\
	PORT_DIPSETTING(    0x0e, DEF_STR( 2C_3C ) )\
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_2C ) )\
	PORT_DIPSETTING(    0x06, DEF_STR( 2C_5C ) )\
	PORT_DIPSETTING(    0x0b, DEF_STR( 1C_3C ) )\
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_4C ) )\
	PORT_DIPSETTING(    0x0d, DEF_STR( 1C_5C ) )\
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )\
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_7C ) )\
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )\
	PORT_DIPNAME( 0xf0, 0xf0, DEF_STR( Coin_B ) )\
	PORT_DIPSETTING(    0x40, DEF_STR( 4C_1C ) )\
	PORT_DIPSETTING(    0xa0, DEF_STR( 3C_1C ) )\
	PORT_DIPSETTING(    0x10, DEF_STR( 2C_1C ) )\
	PORT_DIPSETTING(    0x20, DEF_STR( 3C_2C ) )\
	PORT_DIPSETTING(    0x80, DEF_STR( 4C_3C ) )\
	PORT_DIPSETTING(    0xf0, DEF_STR( 1C_1C ) )\
	PORT_DIPSETTING(    0xc0, DEF_STR( 3C_4C ) )\
	PORT_DIPSETTING(    0xe0, DEF_STR( 2C_3C ) )\
	PORT_DIPSETTING(    0x70, DEF_STR( 1C_2C ) )\
	PORT_DIPSETTING(    0x60, DEF_STR( 2C_5C ) )\
	PORT_DIPSETTING(    0xb0, DEF_STR( 1C_3C ) )\
	PORT_DIPSETTING(    0x30, DEF_STR( 1C_4C ) )\
	PORT_DIPSETTING(    0xd0, DEF_STR( 1C_5C ) )\
	PORT_DIPSETTING(    0x50, DEF_STR( 1C_6C ) )\
	PORT_DIPSETTING(    0x90, DEF_STR( 1C_7C ) )\
	PORT_DIPSETTING(    0x00, "Disable All Coins" )


static INPUT_PORTS_START( amidar )
	AMIDAR_IN0

	AMIDAR_IN1

	PORT_START_TAG("IN2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY PORT_COCKTAIL
	PORT_DIPNAME( 0x02, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "30000 50000" )
	PORT_DIPSETTING(    0x04, "Every 50000" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	AMIDAR_DSW

INPUT_PORTS_END

/* absolutely identical to amidar, the only difference is the BONUS dip switch */
static INPUT_PORTS_START( amidaru )
	PORT_INCLUDE( amidar )

	PORT_MODIFY("IN2")
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "30000 70000" )
	PORT_DIPSETTING(    0x04, "50000 80000" )
INPUT_PORTS_END

static INPUT_PORTS_START( amidaro )
	PORT_INCLUDE( amidar )

	PORT_MODIFY("IN1")
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x03, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x01, "3" )
	PORT_DIPSETTING(    0x00, "4" )

	PORT_MODIFY("IN2")
	PORT_DIPNAME( 0x02, 0x00, "Level Progression" )
	PORT_DIPSETTING(    0x00, "Slow" )
	PORT_DIPSETTING(    0x02, "Fast" )
	PORT_DIPNAME( 0x04, 0x00, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x00, "30000 70000" )
	PORT_DIPSETTING(    0x04, "50000 80000" )
INPUT_PORTS_END

/* similar to Amidar, dip switches are different and port 3, which in Amidar */
/* selects coins per credit, is not used. */
static INPUT_PORTS_START( turtles )
	PORT_START_TAG("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_COCKTAIL
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably space for button 2 */
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START_TAG("IN1")
	PORT_DIPNAME( 0x03, 0x01, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "4" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_DIPSETTING(    0x03, "126 (Cheat)")
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* probably space for player 2 button 2 */
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_COCKTAIL
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_COCKTAIL
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_COCKTAIL
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

	PORT_START_TAG("IN2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_COCKTAIL
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x00, "A 1/1 B 2/1 C 1/1" )
	PORT_DIPSETTING(    0x02, "A 1/2 B 1/1 C 1/2" )
	PORT_DIPSETTING(    0x04, "A 1/3 B 3/1 C 1/3" )
	PORT_DIPSETTING(    0x06, "A 1/4 B 4/1 C 1/4" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Cocktail ) )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/* same as Turtles, but dip switches are different. */
static INPUT_PORTS_START( turpin )
	PORT_START_TAG("IN0")
	PORT_INCLUDE( turtles )

	PORT_MODIFY("IN1")
	PORT_DIPNAME( 0x03, 0x00, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "3" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPSETTING(    0x02, "7" )
	PORT_DIPSETTING(    0x03, "126 (Cheat)")

	PORT_MODIFY("IN2")
	PORT_DIPNAME( 0x06, 0x00, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 1C_2C ) )
INPUT_PORTS_END



static MACHINE_DRIVER_START( amidar )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", Z80, 18432000/6)	/* 3.072 MHz */
	MDRV_CPU_PROGRAM_MAP(readmem,writemem)

	MDRV_CPU_ADD(Z80,14318000/8)
	/* audio CPU */	/* 1.78975 MHz */
	MDRV_CPU_PROGRAM_MAP(amidar_sound_readmem,amidar_sound_writemem)
	MDRV_CPU_IO_MAP(amidar_sound_readport,amidar_sound_writeport)

	MDRV_MACHINE_RESET(scramble)

	/* video hardware */
	MDRV_SCREEN_ADD("main", RASTER)
	MDRV_SCREEN_REFRESH_RATE(16000.0/132/2)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(32*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(0*8, 32*8-1, 2*8, 30*8-1)
	MDRV_GFXDECODE(amidar)
	MDRV_PALETTE_LENGTH(32+64+2+8)

	MDRV_PALETTE_INIT(turtles)
	MDRV_VIDEO_START(turtles)
	MDRV_VIDEO_UPDATE(galaxian)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD(AY8910, 14318000/8)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.16)

	MDRV_SOUND_ADD(AY8910, 14318000/8)
	MDRV_SOUND_CONFIG(amidar_ay8910_interface_2)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.16)
MACHINE_DRIVER_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( amidar )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "amidar.2c",    0x0000, 0x1000, CRC(c294bf27) SHA1(399325bf1559e8cdbddf7cfbf0dc739f9ed72ef0) )
	ROM_LOAD( "amidar.2e",    0x1000, 0x1000, CRC(e6e96826) SHA1(e9c4f8c594640424b456505e676352a98b758c03) )
	ROM_LOAD( "amidar.2f",    0x2000, 0x1000, CRC(3656be6f) SHA1(9d652f66bedcf17a6453c0e0ead30bfd7ea0bd0a) )
	ROM_LOAD( "amidar.2h",    0x3000, 0x1000, CRC(1be170bd) SHA1(c047bc393b297c0d47668a5f6f4870e3fac937ef) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "amidar.5c",    0x0000, 0x1000, CRC(c4b66ae4) SHA1(9d09dbde4019f7be3abe0815b0e06d542c01c255) )
	ROM_LOAD( "amidar.5d",    0x1000, 0x1000, CRC(806785af) SHA1(c8c85e3a6a204feccd7859b4527bd649e96134b4) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "amidar.5f",    0x0000, 0x0800, CRC(5e51e84d) SHA1(dfe84db7e2b1a45a1d484fcf37291f536bc5324c) )
	ROM_LOAD( "amidar.5h",    0x0800, 0x0800, CRC(2f7f1c30) SHA1(83c330eca20dfcc6a4099001943b9ed7a7c3db5b) )

	ROM_REGION( 0x0020, REGION_PROMS, 0 )
	ROM_LOAD( "amidar.clr",   0x0000, 0x0020, CRC(f940dcc3) SHA1(1015e56f37c244a850a8f4bf0e36668f047fd46d) )
ROM_END

ROM_START( amidaru )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "amidarus.2c",  0x0000, 0x1000, CRC(951e0792) SHA1(3a68b829c9ffb465bd6582c9ea566e0e947c6c19) )
	ROM_LOAD( "amidarus.2e",  0x1000, 0x1000, CRC(a1a3a136) SHA1(330ec857fdf4c1b28e2560a5f63a2432f87f9b2f) )
	ROM_LOAD( "amidarus.2f",  0x2000, 0x1000, CRC(a5121bf5) SHA1(fe15b91724758ede43dd332327919f164772c592) )
	ROM_LOAD( "amidarus.2h",  0x3000, 0x1000, CRC(051d1c7f) SHA1(3cfa0f728a5c27da0a3fe2579ad226129ccde232) )
	ROM_LOAD( "amidarus.2j",  0x4000, 0x1000, CRC(351f00d5) SHA1(6659357f40f888b21be00826246200fd3a8a88ce) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "amidarus.5c",  0x0000, 0x1000, CRC(8ca7b750) SHA1(4f4c2915503b85abe141d717fd254ee10c9da99e) )
	ROM_LOAD( "amidarus.5d",  0x1000, 0x1000, CRC(9b5bdc0a) SHA1(84d953618c8bf510d23b42232a856ac55f1baff5) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "amidarus.5f",  0x0000, 0x0800, CRC(2cfe5ede) SHA1(0d86a78008ac8653c17fff5be5ebdf1f0a9d31eb) )
	ROM_LOAD( "amidarus.5h",  0x0800, 0x0800, CRC(57c4fd0d) SHA1(8764deec9fbff4220d61df621b12fc36c3702601) )

	ROM_REGION( 0x0020, REGION_PROMS, 0 )
	ROM_LOAD( "amidar.clr",   0x0000, 0x0020, CRC(f940dcc3) SHA1(1015e56f37c244a850a8f4bf0e36668f047fd46d) )
ROM_END

ROM_START( amidaro )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "107.2cd",      0x0000, 0x1000, CRC(c52536be) SHA1(3f64578214d2d9f0e4e7ee87e09b0aac33a73098) )
	ROM_LOAD( "108.2fg",      0x1000, 0x1000, CRC(38538b98) SHA1(12b2a0c09926d006781bee5d450bc0c391cc1fb5) )
	ROM_LOAD( "109.2fg",      0x2000, 0x1000, CRC(69907f0f) SHA1(f1d19a76ffc41ee8c5c574f10108cfdfe525b732) )
	ROM_LOAD( "110.2h",       0x3000, 0x1000, CRC(ba149a93) SHA1(9ef1d27f0780612be0ea2be94c3a2c781a4924c8) )
	ROM_LOAD( "111.2j",       0x4000, 0x1000, CRC(20d01c2e) SHA1(e09437ff440f04036d5ec74b355e97bbbbfefb95) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "amidarus.5c",  0x0000, 0x1000, CRC(8ca7b750) SHA1(4f4c2915503b85abe141d717fd254ee10c9da99e) )
	ROM_LOAD( "amidarus.5d",  0x1000, 0x1000, CRC(9b5bdc0a) SHA1(84d953618c8bf510d23b42232a856ac55f1baff5) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "amidarus.5f",  0x0000, 0x0800, CRC(2cfe5ede) SHA1(0d86a78008ac8653c17fff5be5ebdf1f0a9d31eb) )
	ROM_LOAD( "113.5h",       0x0800, 0x0800, CRC(bcdce168) SHA1(e593d03c460ef4607e3ba25019d9f01d4a717dd9) )  /* The letter 'S' is slightly different */

	ROM_REGION( 0x0020, REGION_PROMS, 0 )
	ROM_LOAD( "amidar.clr",   0x0000, 0x0020, CRC(f940dcc3) SHA1(1015e56f37c244a850a8f4bf0e36668f047fd46d) )
ROM_END

ROM_START( amidarb )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "ami2gor.2c", 0x0000, 0x1000, CRC(9ad2dcd2) SHA1(43ceb93d891c1ebf55e7c26de13e3db8e1d26f6d) )
	ROM_LOAD( "2.2f",       0x1000, 0x1000, CRC(66282ff5) SHA1(986778278eb339768d190460680e7aa698812488) )
	ROM_LOAD( "3.2j",       0x2000, 0x1000, CRC(b0860e31) SHA1(8fb92b0e71c826a509a8f712553de0f4a636286f) )
	ROM_LOAD( "4.2m",       0x3000, 0x1000, CRC(4a4086c9) SHA1(6f309b67dc68e06e6eb1d3ee2ae75afe253a4ce3) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "8.11d",      0x0000, 0x1000, CRC(8ca7b750) SHA1(4f4c2915503b85abe141d717fd254ee10c9da99e) )
	ROM_LOAD( "9.9d",       0x1000, 0x1000, CRC(9b5bdc0a) SHA1(84d953618c8bf510d23b42232a856ac55f1baff5) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "5.5f",      0x0000, 0x0800, CRC(2082ad0a) SHA1(c6014d9575e92adf09b0961c2158a779ebe940c4) )
	ROM_LOAD( "6.5h",      0x0800, 0x0800, CRC(3029f94f) SHA1(3b432b42e79f8b0a7d65e197f373a04e3c92ff20) )

	ROM_REGION( 0x0020, REGION_PROMS, 0 )
	ROM_LOAD( "n82s123n.6e",   0x0000, 0x0020, CRC(01004d3f) SHA1(e53cbc54ea96e846481a67bbcccf6b1726e70f9c) )
ROM_END

ROM_START( amigo )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "2732.a1",      0x0000, 0x1000, CRC(930dc856) SHA1(7022f1f26830baccdc8b8f0b10fb1d1ccb080f22) )
	ROM_LOAD( "2732.a2",      0x1000, 0x1000, CRC(66282ff5) SHA1(986778278eb339768d190460680e7aa698812488) )
	ROM_LOAD( "2732.a3",      0x2000, 0x1000, CRC(e9d3dc76) SHA1(627c6068c65985175388aec43ac2a4248b004c97) )
	ROM_LOAD( "2732.a4",      0x3000, 0x1000, CRC(4a4086c9) SHA1(6f309b67dc68e06e6eb1d3ee2ae75afe253a4ce3) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "amidarus.5c",  0x0000, 0x1000, CRC(8ca7b750) SHA1(4f4c2915503b85abe141d717fd254ee10c9da99e) )
	ROM_LOAD( "amidarus.5d",  0x1000, 0x1000, CRC(9b5bdc0a) SHA1(84d953618c8bf510d23b42232a856ac55f1baff5) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "2716.a6",      0x0000, 0x0800, CRC(2082ad0a) SHA1(c6014d9575e92adf09b0961c2158a779ebe940c4) )
	ROM_LOAD( "2716.a5",      0x0800, 0x0800, CRC(3029f94f) SHA1(3b432b42e79f8b0a7d65e197f373a04e3c92ff20) )

	ROM_REGION( 0x0020, REGION_PROMS, 0 )
	ROM_LOAD( "amidar.clr",   0x0000, 0x0020, CRC(f940dcc3) SHA1(1015e56f37c244a850a8f4bf0e36668f047fd46d) )
ROM_END

ROM_START( turtles )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "turt_vid.2c",  0x0000, 0x1000, CRC(ec5e61fb) SHA1(3ca89800fda7a7e61f54d71d5302908be2706def) )
	ROM_LOAD( "turt_vid.2e",  0x1000, 0x1000, CRC(fd10821e) SHA1(af74602bf2454eb8f3b9bb5c425e2476feeecd69) )
	ROM_LOAD( "turt_vid.2f",  0x2000, 0x1000, CRC(ddcfc5fa) SHA1(2af9383e5a289c2d7fbe6cf5e5b1519c352afbab) )
	ROM_LOAD( "turt_vid.2h",  0x3000, 0x1000, CRC(9e71696c) SHA1(3dcdf5dc601c875fc9d8b9a46e3ef588e7478e0d) )
	ROM_LOAD( "turt_vid.2j",  0x4000, 0x1000, CRC(fcd49fef) SHA1(bb1e91b2e6d4b5a861bf37907ef6b198328d8d83) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "turt_snd.5c",  0x0000, 0x1000, CRC(f0c30f9a) SHA1(5621f336e9be8acf986a34bbb8855ed5d45c28ef) )
	ROM_LOAD( "turt_snd.5d",  0x1000, 0x1000, CRC(af5fc43c) SHA1(8a49c55feba094b07380615cf0b6f0878c25a260) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "turt_vid.5h",  0x0000, 0x0800, CRC(e5999d52) SHA1(bc3f52cf6c6e19dfd2dacd1e8c9128f437e995fc) )
	ROM_LOAD( "turt_vid.5f",  0x0800, 0x0800, CRC(c3ffd655) SHA1(dee51d77be262a2944488e381541c10a2b6e5d83) )

	ROM_REGION( 0x0020, REGION_PROMS, 0 )
	ROM_LOAD( "turtles.clr",  0x0000, 0x0020, CRC(f3ef02dd) SHA1(09fd795170d7d30f101d579f57553da5ff3800ab) )
ROM_END

ROM_START( turpin )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "m1",           0x0000, 0x1000, CRC(89177473) SHA1(0717b1e7308ffe527edfc578ec4353809e7d9eea) )
	ROM_LOAD( "m2",           0x1000, 0x1000, CRC(4c6ca5c6) SHA1(dd4ca7adaa523a8e775cdfaa99bb3cc25da32c08) )
	ROM_LOAD( "m3",           0x2000, 0x1000, CRC(62291652) SHA1(82965d3e9608afde4ff06cba1d7a4b11cd904c11) )
	ROM_LOAD( "turt_vid.2h",  0x3000, 0x1000, CRC(9e71696c) SHA1(3dcdf5dc601c875fc9d8b9a46e3ef588e7478e0d) )
	ROM_LOAD( "m5",           0x4000, 0x1000, CRC(7d2600f2) SHA1(1a9bdf63b50419c6e0d9c401c3dcf29d5b459fa6) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "turt_snd.5c",  0x0000, 0x1000, CRC(f0c30f9a) SHA1(5621f336e9be8acf986a34bbb8855ed5d45c28ef) )
	ROM_LOAD( "turt_snd.5d",  0x1000, 0x1000, CRC(af5fc43c) SHA1(8a49c55feba094b07380615cf0b6f0878c25a260) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "turt_vid.5h",  0x0000, 0x0800, CRC(e5999d52) SHA1(bc3f52cf6c6e19dfd2dacd1e8c9128f437e995fc) )
	ROM_LOAD( "turt_vid.5f",  0x0800, 0x0800, CRC(c3ffd655) SHA1(dee51d77be262a2944488e381541c10a2b6e5d83) )

	ROM_REGION( 0x0020, REGION_PROMS, 0 )
	ROM_LOAD( "turtles.clr",  0x0000, 0x0020, CRC(f3ef02dd) SHA1(09fd795170d7d30f101d579f57553da5ff3800ab) )
ROM_END

ROM_START( 600 )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )
	ROM_LOAD( "600_vid.2c",   0x0000, 0x1000, CRC(8ee090ae) SHA1(3d491313da6cccd6dbc15774569be0555fe2f73a) )
	ROM_LOAD( "600_vid.2e",   0x1000, 0x1000, CRC(45bfaff2) SHA1(ba4f7aa499f4993ec2191b8832b5604fd41964bc) )
	ROM_LOAD( "600_vid.2f",   0x2000, 0x1000, CRC(9f4c8ed7) SHA1(2564dae82019097227351a7ddc9c5156ca00297a) )
	ROM_LOAD( "600_vid.2h",   0x3000, 0x1000, CRC(a92ef056) SHA1(c319d41a3345b84670fe9110f78332c1cfe1e163) )
	ROM_LOAD( "600_vid.2j",   0x4000, 0x1000, CRC(6dadd72d) SHA1(5602b5ebb2c287f72a5ce873b4e3dfd19b8412a0) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )
	ROM_LOAD( "600_snd.5c",   0x0000, 0x1000, CRC(1773c68e) SHA1(cc4aa3a98e85bc6300f8c1ee1a0448071d7c6dfa) )
	ROM_LOAD( "600_snd.5d",   0x1000, 0x1000, CRC(a311b998) SHA1(39af321b8c3f211ed6d083a2aba4fbc8af11c9e8) )

	ROM_REGION( 0x1000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "600_vid.5h",   0x0000, 0x0800, CRC(006c3d56) SHA1(0c773e0e84d0bf45be5a5a7cfff960c1ca2f0320) )
	ROM_LOAD( "600_vid.5f",   0x0800, 0x0800, CRC(7dbc0426) SHA1(29eeb3cdb5a3bcf7115d8099e4d04cf76216b003) )

	ROM_REGION( 0x0020, REGION_PROMS, 0 )
	ROM_LOAD( "turtles.clr",  0x0000, 0x0020, CRC(f3ef02dd) SHA1(09fd795170d7d30f101d579f57553da5ff3800ab) )
ROM_END



GAME( 1981, amidar,  0,       amidar, amidar,  amidar,       ROT90, "Konami", "Amidar", GAME_SUPPORTS_SAVE )
GAME( 1982, amidaru, amidar,  amidar, amidaru, amidar,       ROT90, "Konami (Stern license)", "Amidar (Stern)", GAME_SUPPORTS_SAVE )
GAME( 1982, amidaro, amidar,  amidar, amidaro, amidar,       ROT90, "Konami (Olympia license)", "Amidar (Olympia)", GAME_SUPPORTS_SAVE )
GAME( 1982, amidarb, amidar,  amidar, amidaru, amidar,       ROT90, "bootleg", "Amidar (Bootleg)", GAME_SUPPORTS_SAVE ) /* Simular to Amigo bootleg */
GAME( 1982, amigo,   amidar,  amidar, amidaru, amidar,       ROT90, "bootleg", "Amigo", GAME_SUPPORTS_SAVE )
GAME( 1981, turtles, 0,       amidar, turtles, scramble_ppi, ROT90, "[Konami] (Stern license)", "Turtles", GAME_SUPPORTS_SAVE )
GAME( 1981, turpin,  turtles, amidar, turpin,  scramble_ppi, ROT90, "[Konami] (Sega license)", "Turpin", GAME_SUPPORTS_SAVE )
GAME( 1981, 600,     turtles, amidar, turtles, scramble_ppi, ROT90, "Konami", "600", GAME_SUPPORTS_SAVE )
