/******************************************************************************

Quiz DNA no Hanran (c) 1992 Face
Quiz Gakuen Paradise (c) 1991 NMK
Quiz Gekiretsu Scramble (Gakuen Paradise 2) (c) 1993 Face

Video hardware
    driver by Uki

******************************************************************************/

#include "emu.h"
#include "includes/quizdna.h"


static TILE_GET_INFO( get_bg_tile_info )
{
	quizdna_state *state = machine->driver_data<quizdna_state>();
	int code = state->bg_ram[tile_index*2] + state->bg_ram[tile_index*2+1]*0x100 ;
	int col = state->bg_ram[tile_index*2+0x1000] & 0x7f;

	if (code>0x7fff)
		code &= 0x83ff;

	SET_TILE_INFO(1, code, col, 0);
}

static TILE_GET_INFO( get_fg_tile_info )
{
	quizdna_state *state = machine->driver_data<quizdna_state>();
	int code,col,x,y;
	UINT8 *FG = machine->region("user1")->base();

	x = tile_index & 0x1f;
	y = FG[(tile_index >> 5) & 0x1f] & 0x3f;
	code = y & 1;

	y >>= 1;

	col = state->fg_ram[x*2 + y*0x40 + 1];
	code += (state->fg_ram[x*2 + y*0x40] + (col & 0x1f) * 0x100) * 2;
	col >>= 5;
	col = (col & 3) | ((col & 4) << 1);

	SET_TILE_INFO(0, code, col, 0);
}


VIDEO_START( quizdna )
{
	quizdna_state *state = machine->driver_data<quizdna_state>();
	state->flipscreen = -1;
	state->video_enable = 0;
	state->bg_xscroll[0] = 0;
	state->bg_xscroll[1] = 0;

	state->bg_ram = auto_alloc_array(machine, UINT8, 0x2000);
	state->fg_ram = auto_alloc_array(machine, UINT8, 0x1000);

	state->bg_tilemap = tilemap_create( machine, get_bg_tile_info,tilemap_scan_rows,8,8,64,32 );
	state->fg_tilemap = tilemap_create( machine, get_fg_tile_info,tilemap_scan_rows,16,8,32,32 );

	tilemap_set_transparent_pen( state->fg_tilemap,0 );
}

WRITE8_HANDLER( quizdna_bg_ram_w )
{
	quizdna_state *state = space->machine->driver_data<quizdna_state>();
	UINT8 *RAM = space->machine->region("maincpu")->base();
	state->bg_ram[offset] = data;
	RAM[0x12000+offset] = data;

	tilemap_mark_tile_dirty(state->bg_tilemap, (offset & 0xfff) / 2 );
}

WRITE8_HANDLER( quizdna_fg_ram_w )
{
	quizdna_state *state = space->machine->driver_data<quizdna_state>();
	int i;
	int offs = offset & 0xfff;
	UINT8 *RAM = space->machine->region("maincpu")->base();

	RAM[0x10000+offs] = data;
	RAM[0x11000+offs] = data; /* mirror */
	state->fg_ram[offs] = data;

	for (i=0; i<32; i++)
		tilemap_mark_tile_dirty(state->fg_tilemap, ((offs/2) & 0x1f) + i*0x20 );
}

WRITE8_HANDLER( quizdna_bg_yscroll_w )
{
	quizdna_state *state = space->machine->driver_data<quizdna_state>();
	tilemap_set_scrolldy( state->bg_tilemap, 255-data, 255-data+1 );
}

WRITE8_HANDLER( quizdna_bg_xscroll_w )
{
	quizdna_state *state = space->machine->driver_data<quizdna_state>();
	int x;
	state->bg_xscroll[offset] = data;
	x = ~(state->bg_xscroll[0] + state->bg_xscroll[1]*0x100) & 0x1ff;

	tilemap_set_scrolldx( state->bg_tilemap, x+64, x-64+10 );
}

WRITE8_HANDLER( quizdna_screen_ctrl_w )
{
	quizdna_state *state = space->machine->driver_data<quizdna_state>();
	int tmp = (data & 0x10) >> 4;
	state->video_enable = data & 0x20;

	coin_counter_w(space->machine, 0, data & 1);

	if (state->flipscreen == tmp)
		return;

	state->flipscreen = tmp;

	flip_screen_set(space->machine, tmp);
	tilemap_set_scrolldx( state->fg_tilemap, 64, -64 +16);
}

WRITE8_HANDLER( paletteram_xBGR_RRRR_GGGG_BBBB_w )
{
	int r,g,b,d0,d1;
	int offs = offset & ~1;

	space->machine->generic.paletteram.u8[offset] = data;

	d0 = space->machine->generic.paletteram.u8[offs];
	d1 = space->machine->generic.paletteram.u8[offs+1];

	r = ((d1 << 1) & 0x1e) | ((d1 >> 4) & 1);
	g = ((d0 >> 3) & 0x1e) | ((d1 >> 5) & 1);
	b = ((d0 << 1) & 0x1e) | ((d1 >> 6) & 1);

	palette_set_color_rgb(space->machine,offs/2,pal5bit(r),pal5bit(g),pal5bit(b));
}

static void draw_sprites(running_machine *machine, bitmap_t *bitmap, const rectangle *cliprect)
{
	quizdna_state *state = machine->driver_data<quizdna_state>();
	UINT8 *spriteram = state->spriteram;
	int offs;

	for (offs = 0; offs<state->spriteram_size; offs+=8)
	{
		int i;

		int x = spriteram[offs + 3]*0x100 + spriteram[offs + 2] + 64 - 8;
		int y = (spriteram[offs + 1] & 1)*0x100 + spriteram[offs + 0];
		int code = (spriteram[offs + 5] * 0x100 + spriteram[offs + 4]) & 0x3fff;
		int col =  spriteram[offs + 6];
		int fx = col & 0x80;
		int fy = col & 0x40;
		int ysize = (spriteram[offs + 1] & 0xc0) >> 6;
		int dy = 0x10;
		col &= 0x1f;

		if (state->flipscreen)
		{
			x -= 7;
			y += 1;
		}

		x &= 0x1ff;
		if (x>0x1f0)
			x -= 0x200;

		if (fy)
		{
			dy = -0x10;
			y += 0x10 * ysize;
		}

		if (code >= 0x2100)
			code &= 0x20ff;

		for (i=0; i<ysize+1; i++)
		{
			y &= 0x1ff;

			drawgfx_transpen(bitmap,cliprect,machine->gfx[2],
					code ^ i,
					col,
					fx,fy,
					x,y,0);

			y += dy;
		}
	}
}

SCREEN_UPDATE( quizdna )
{
	quizdna_state *state = screen->machine->driver_data<quizdna_state>();
	if (state->video_enable)
	{
		tilemap_draw(bitmap, cliprect, state->bg_tilemap, 0, 0);
		draw_sprites(screen->machine, bitmap, cliprect);
		tilemap_draw(bitmap, cliprect, state->fg_tilemap, 0, 0);
	}
	else
		bitmap_fill(bitmap, cliprect, get_black_pen(screen->machine));
	return 0;
}
