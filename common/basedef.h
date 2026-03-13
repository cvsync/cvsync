/*-
 * This software is released under the BSD License, see LICENSE.
 */

#ifndef CVSYNC_BASEDEF_H
#define	CVSYNC_BASEDEF_H

#define	GetWord(w)	((uint16_t)((((uint16_t)((w)[0])) << 8) | \
				    (((uint16_t)((w)[1])) << 0)))

#define	GetDWord(w)	((uint32_t)((((uint32_t)((w)[0])) << 24) | \
				    (((uint32_t)((w)[1])) << 16) | \
				    (((uint32_t)((w)[2])) <<  8) | \
				    (((uint32_t)((w)[3])) <<  0)))

#define	GetDDWord(w)	((uint64_t)((((uint64_t)((w)[0])) << 56) | \
				    (((uint64_t)((w)[1])) << 48) | \
				    (((uint64_t)((w)[2])) << 40) | \
				    (((uint64_t)((w)[3])) << 32) | \
				    (((uint64_t)((w)[4])) << 24) | \
				    (((uint64_t)((w)[5])) << 16) | \
				    (((uint64_t)((w)[6])) <<  8) | \
				    (((uint64_t)((w)[7])) <<  0)))

#define	SetWord(w, v) \
	do { \
		(w)[0] = (uint8_t)((uint16_t)(v) >> 8); \
		(w)[1] = (uint8_t)((uint16_t)(v) >> 0); \
	} while (/* CONSTCOND */ 0)

#define	SetDWord(w, v) \
	do { \
		(w)[0] = (uint8_t)((uint32_t)(v) >> 24); \
		(w)[1] = (uint8_t)((uint32_t)(v) >> 16); \
		(w)[2] = (uint8_t)((uint32_t)(v) >>  8); \
		(w)[3] = (uint8_t)((uint32_t)(v) >>  0); \
	} while (/* CONSTCOND */ 0)

#define	SetDDWord(w, v) \
	do { \
		(w)[0] = (uint8_t)((uint64_t)(v) >> 56); \
		(w)[1] = (uint8_t)((uint64_t)(v) >> 48); \
		(w)[2] = (uint8_t)((uint64_t)(v) >> 40); \
		(w)[3] = (uint8_t)((uint64_t)(v) >> 32); \
		(w)[4] = (uint8_t)((uint64_t)(v) >> 24); \
		(w)[5] = (uint8_t)((uint64_t)(v) >> 16); \
		(w)[6] = (uint8_t)((uint64_t)(v) >>  8); \
		(w)[7] = (uint8_t)((uint64_t)(v) >>  0); \
	} while (/* CONSTCOND */ 0)

#endif /* CVSYNC_BASEDEF_H */
