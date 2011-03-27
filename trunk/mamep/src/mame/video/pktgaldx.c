#include "emu.h"
#include "video/deco16ic.h"
#include "includes/pktgaldx.h"
#include "video/decospr.h"

/* Video on the orginal */

SCREEN_UPDATE( pktgaldx )
{
	pktgaldx_state *state = screen->machine->driver_data<pktgaldx_state>();
	UINT16 flip = deco16ic_pf12_control_r(state->deco16ic, 0, 0xffff);

	flip_screen_set(screen->machine, BIT(flip, 7));
	deco16ic_pf12_update(state->deco16ic, state->pf1_rowscroll, state->pf2_rowscroll);

	bitmap_fill(bitmap, cliprect, 0); /* not Confirmed */
	bitmap_fill(screen->machine->priority_bitmap, NULL, 0);

	deco16ic_tilemap_2_draw(state->deco16ic, bitmap, cliprect, 0, 0);
	screen->machine->device<decospr_device>("spritegen")->draw_sprites(screen->machine, bitmap, cliprect, state->spriteram, 0x400, true);
	deco16ic_tilemap_1_draw(state->deco16ic, bitmap, cliprect, 0, 0);
	return 0;
}

/* Video for the bootleg */

SCREEN_UPDATE( pktgaldb )
{
	pktgaldx_state *state = screen->machine->driver_data<pktgaldx_state>();
	int x, y;
	int offset = 0;
	int tileno;
	int colour;

	bitmap_fill(bitmap, cliprect, get_black_pen(screen->machine));

	/* the bootleg seems to treat the tilemaps as sprites */
	for (offset = 0; offset < 0x1600 / 2; offset += 8)
	{
		tileno = state->pktgaldb_sprites[offset + 3] | (state->pktgaldb_sprites[offset + 2] << 16);
		colour = state->pktgaldb_sprites[offset + 1] >> 1;
		x = state->pktgaldb_sprites[offset + 0];
		y = state->pktgaldb_sprites[offset + 4];

		x -= 0xc2;
		y &= 0x1ff;
		y -= 8;

		drawgfx_transpen(bitmap, cliprect, screen->machine->gfx[0], tileno ^ 0x1000, colour, 0, 0, x, y, 0);
	}

	for (offset = 0x1600/2; offset < 0x2000 / 2; offset += 8)
	{
		tileno = state->pktgaldb_sprites[offset + 3] | (state->pktgaldb_sprites[offset + 2] << 16);
		colour = state->pktgaldb_sprites[offset + 1] >> 1;
		x = state->pktgaldb_sprites[offset + 0] & 0x1ff;
		y = state->pktgaldb_sprites[offset + 4] & 0x0ff;

		x -= 0xc2;
		y &= 0x1ff;
		y -= 8;

		drawgfx_transpen(bitmap, cliprect, screen->machine->gfx[0], tileno ^ 0x4000, colour, 0, 0, x, y, 0);
	}

	for (offset = 0x2000/2; offset < 0x4000 / 2; offset += 8)
	{
		tileno = state->pktgaldb_sprites[offset + 3] | (state->pktgaldb_sprites[offset + 2] << 16);
		colour = state->pktgaldb_sprites[offset + 1] >> 1;
		x = state->pktgaldb_sprites[offset + 0] & 0x1ff;
		y = state->pktgaldb_sprites[offset + 4] & 0x0ff;

		x -= 0xc2;
		y &= 0x1ff;
		y -= 8;

		drawgfx_transpen(bitmap, cliprect, screen->machine->gfx[0], tileno ^ 0x3000, colour, 0, 0, x, y, 0);
	}

	return 0;
}
