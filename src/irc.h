
/* 
 * irc.h -- part of irc.mod
 * 
 * $Id: irc.h,v 1.10 2000/01/22 23:31:54 per Exp $
 */

/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _EGG_MOD_IRC_IRC_H
#define _EGG_MOD_IRC_IRC_H

#define check_tcl_join(a,b,c,d) check_tcl_joinpart(a,b,c,d,H_join)
#define check_tcl_part(a,b,c,d) check_tcl_joinpart(a,b,c,d,H_part)
#define check_tcl_splt(a,b,c,d) check_tcl_joinpart(a,b,c,d,H_splt)
#define check_tcl_rejn(a,b,c,d) check_tcl_joinpart(a,b,c,d,H_rejn)
#define check_tcl_sign(a,b,c,d,e) check_tcl_signtopcnick(a,b,c,d,e,H_sign)
#define check_tcl_topc(a,b,c,d,e) check_tcl_signtopcnick(a,b,c,d,e,H_topc)
#define check_tcl_nick(a,b,c,d,e) check_tcl_signtopcnick(a,b,c,d,e,H_nick)
#define check_tcl_mode(a,b,c,d,e,f) check_tcl_kickmode(a,b,c,d,e,f,H_mode)
#define check_tcl_kick(a,b,c,d,e,f) check_tcl_kickmode(a,b,c,d,e,f,H_kick)

#define REVENGE_KICK 1		/* kicked victim */
#define REVENGE_DEOP 2		/* took op */

/* reset(bans|exempts|invites) are now just macros that call resetmasks
 * in order to reduce the code duplication. <cybah>
 */
#define resetbans(chan)         resetmasks((chan), (chan)->channel.ban, (chan)->bans, global_bans, 'b')
#define resetexempts(chan)      resetmasks((chan), (chan)->channel.exempt, (chan)->exempts, global_exempts, 'e')
#define resetinvites(chan)      resetmasks((chan), (chan)->channel.invite, (chan)->invites, global_invites, 'I')

#define newban(chan, mask, who)         newmask((chan)->channel.ban, mask, who)
#define newexempt(chan, mask, who)      newmask((chan)->channel.exempt, mask, who)
#define newinvite(chan, mask, who)      newmask((chan)->channel.invite, mask, who)

#endif /* _EGG_MOD_IRC_IRC_H */
