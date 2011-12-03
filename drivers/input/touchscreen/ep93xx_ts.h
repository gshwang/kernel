/*
 * ep93xx_ts.h
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License
 * at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and
 * limitations under the License.
 *
 * The initial developer of the original code is David A. Hinds
 * <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
 * are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License version 2 (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of the
 * above.  If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use
 * your version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the MPL or the GPL.
 */

#ifndef _LINUX_EP93XX_TS_H
#define _LINUX_EP93XX_TS_H

/*touchscreen register defines*/
#define SYSCON_KTDIV 	EP93XX_SYSCON_KEY_TOUCH_CLOCK_DIV
#define SYSCON_SWLOCK	EP93XX_SYSCON_SWLOCK
#define TSSetup 	EP93XX_TOUCHSCREEN_TSSetup
#define TSXYMaxMin	EP93XX_TOUCHSCREEN_TSXYMaxMin
#define TSXYResult	EP93XX_TOUCHSCREEN_TSDischarge
#define TSDischarge	EP93XX_TOUCHSCREEN_TSDischarge
#define TSXSample	EP93XX_TOUCHSCREEN_TSXSample
#define TSYSample	EP93XX_TOUCHSCREEN_TSYSample
#define TSDirect	EP93XX_TOUCHSCREEN_TSDirect
#define TSDetect	EP93XX_TOUCHSCREEN_TSDetect
#define TSSWLock	EP93XX_TOUCHSCREEN_TSSWLock
#define TSSetup2	EP93XX_TOUCHSCREEN_TSSetup2


#define SYSCON_DEVCFG	EP93XX_SYSCON_DEVICE_CONFIG
#define TIMER2CONTROL	EP93XX_TIMER2_CONTROL
#define TIMER2LOAD	EP93XX_TIMER2_LOAD
#define TIMER2VALUE	EP93XX_TIMER2_VALUE
#define TIMER2CLEAR  	EP93XX_TIMER2_CLEAR
#endif
