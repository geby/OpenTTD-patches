/* $Id$ */

/** @file namegen.cpp Town name generators. */

#include "stdafx.h"
#include "namegen_func.h"
#include "string_func.h"

#include "table/namegen.h"

static inline uint32 SeedChance(int shift_by, int max, uint32 seed)
{
	return (GB(seed, shift_by, 16) * max) >> 16;
}

static inline uint32 SeedModChance(int shift_by, int max, uint32 seed)
{
	/* This actually gives *MUCH* more even distribution of the values
	 * than SeedChance(), which is absolutely horrible in that. If
	 * you do not believe me, try with i.e. the Czech town names,
	 * compare the words (nicely visible on prefixes) generated by
	 * SeedChance() and SeedModChance(). Do not get dicouraged by the
	 * never-use-modulo myths, which hold true only for the linear
	 * congruential generators (and Random() isn't such a generator).
	 * --pasky
	 * TODO: Perhaps we should use it for all the name generators? --pasky */
	return (seed >> shift_by) % max;
}

static inline int32 SeedChanceBias(int shift_by, int max, uint32 seed, int bias)
{
	return SeedChance(shift_by, max + bias, seed) - bias;
}

static void ReplaceWords(const char *org, const char *rep, char *buf)
{
	if (strncmp(buf, org, 4) == 0) strncpy(buf, rep, 4); // Safe as the string in buf is always more than 4 characters long.
}

static byte MakeEnglishOriginalTownName(char *buf, uint32 seed, const char *last)
{
	int i;

	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	/* optional first segment */
	i = SeedChanceBias(0, lengthof(name_original_english_1), seed, 50);
	if (i >= 0)
		strecat(buf, name_original_english_1[i], last);

	/* mandatory middle segments */
	strecat(buf, name_original_english_2[SeedChance(4,  lengthof(name_original_english_2), seed)], last);
	strecat(buf, name_original_english_3[SeedChance(7,  lengthof(name_original_english_3), seed)], last);
	strecat(buf, name_original_english_4[SeedChance(10, lengthof(name_original_english_4), seed)], last);
	strecat(buf, name_original_english_5[SeedChance(13, lengthof(name_original_english_5), seed)], last);

	/* optional last segment */
	i = SeedChanceBias(15, lengthof(name_original_english_6), seed, 60);
	if (i >= 0)
		strecat(buf, name_original_english_6[i], last);

	if (buf[0] == 'C' && (buf[1] == 'e' || buf[1] == 'i'))
		buf[0] = 'K';

	ReplaceWords("Cunt", "East", buf);
	ReplaceWords("Slag", "Pits", buf);
	ReplaceWords("Slut", "Edin", buf);
	//ReplaceWords("Fart", "Boot", buf);
	ReplaceWords("Drar", "Quar", buf);
	ReplaceWords("Dreh", "Bash", buf);
	ReplaceWords("Frar", "Shor", buf);
	ReplaceWords("Grar", "Aber", buf);
	ReplaceWords("Brar", "Over", buf);
	ReplaceWords("Wrar", "Inve", buf);

	return 0;
}


static byte MakeEnglishAdditionalTownName(char *buf, uint32 seed, const char *last)
{
	int i;

	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	/* optional first segment */
	i = SeedChanceBias(0, lengthof(name_additional_english_prefix), seed, 50);
	if (i >= 0)
		strecat(buf, name_additional_english_prefix[i], last);

	if (SeedChance(3, 20, seed) >= 14) {
		strecat(buf, name_additional_english_1a[SeedChance(6, lengthof(name_additional_english_1a), seed)], last);
	} else {
		strecat(buf, name_additional_english_1b1[SeedChance(6, lengthof(name_additional_english_1b1), seed)], last);
		strecat(buf, name_additional_english_1b2[SeedChance(9, lengthof(name_additional_english_1b2), seed)], last);
		if (SeedChance(11, 20, seed) >= 4) {
			strecat(buf, name_additional_english_1b3a[SeedChance(12, lengthof(name_additional_english_1b3a), seed)], last);
		} else {
			strecat(buf, name_additional_english_1b3b[SeedChance(12, lengthof(name_additional_english_1b3b), seed)], last);
		}
	}

	strecat(buf, name_additional_english_2[SeedChance(14, lengthof(name_additional_english_2), seed)], last);

	/* optional last segment */
	i = SeedChanceBias(15, lengthof(name_additional_english_3), seed, 60);
	if (i >= 0)
		strecat(buf, name_additional_english_3[i], last);

	ReplaceWords("Cunt", "East", buf);
	ReplaceWords("Slag", "Pits", buf);
	ReplaceWords("Slut", "Edin", buf);
	ReplaceWords("Fart", "Boot", buf);
	ReplaceWords("Drar", "Quar", buf);
	ReplaceWords("Dreh", "Bash", buf);
	ReplaceWords("Frar", "Shor", buf);
	ReplaceWords("Grar", "Aber", buf);
	ReplaceWords("Brar", "Over", buf);
	ReplaceWords("Wrar", "Stan", buf);

	return 0;
}

static byte MakeAustrianTownName(char *buf, uint32 seed, const char *last)
{
	int i, j = 0;
	strecpy(buf, "", last);

	/* Bad, Maria, Gross, ... */
	i = SeedChanceBias(0, lengthof(name_austrian_a1), seed, 15);
	if (i >= 0) strecat(buf, name_austrian_a1[i], last);

	i = SeedChance(4, 6, seed);
	if (i >= 4) {
		/* Kaisers-kirchen */
		strecat(buf, name_austrian_a2[SeedChance( 7, lengthof(name_austrian_a2), seed)], last);
		strecat(buf, name_austrian_a3[SeedChance(13, lengthof(name_austrian_a3), seed)], last);
	} else if (i >= 2) {
		/* St. Johann */
		strecat(buf, name_austrian_a5[SeedChance( 7, lengthof(name_austrian_a5), seed)], last);
		strecat(buf, name_austrian_a6[SeedChance( 9, lengthof(name_austrian_a6), seed)], last);
		j = 1; // More likely to have a " an der " or " am "
	} else {
		/* Zell */
		strecat(buf, name_austrian_a4[SeedChance( 7, lengthof(name_austrian_a4), seed)], last);
	}

	i = SeedChance(1, 6, seed);
	if (i >= 4 - j) {
		/* an der Donau (rivers) */
		strecat(buf, name_austrian_f1[SeedChance(4, lengthof(name_austrian_f1), seed)], last);
		strecat(buf, name_austrian_f2[SeedChance(5, lengthof(name_austrian_f2), seed)], last);
	} else if (i >= 2 - j) {
		/* am Dachstein (mountains) */
		strecat(buf, name_austrian_b1[SeedChance(4, lengthof(name_austrian_b1), seed)], last);
		strecat(buf, name_austrian_b2[SeedChance(5, lengthof(name_austrian_b2), seed)], last);
	}

	return 0;
}

static byte MakeGermanTownName(char *buf, uint32 seed, const char *last)
{
	uint i;
	uint seed_derivative;

	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	seed_derivative = SeedChance(7, 28, seed);

	/* optional prefix */
	if (seed_derivative == 12 || seed_derivative == 19) {
		i = SeedChance(2, lengthof(name_german_pre), seed);
		strecat(buf, name_german_pre[i], last);
	}

	/* mandatory middle segments including option of hardcoded name */
	i = SeedChance(3, lengthof(name_german_real) + lengthof(name_german_1), seed);
	if (i < lengthof(name_german_real)) {
		strecat(buf, name_german_real[i], last);
	} else {
		strecat(buf, name_german_1[i - lengthof(name_german_real)], last);

		i = SeedChance(5, lengthof(name_german_2), seed);
		strecat(buf, name_german_2[i], last);
	}

	/* optional suffix */
	if (seed_derivative == 24) {
		i = SeedChance(9,
			lengthof(name_german_4_an_der) + lengthof(name_german_4_am), seed);
		if (i < lengthof(name_german_4_an_der)) {
			strecat(buf, name_german_3_an_der[0], last);
			strecat(buf, name_german_4_an_der[i], last);
		} else {
			strecat(buf, name_german_3_am[0], last);
			strecat(buf, name_german_4_am[i - lengthof(name_german_4_an_der)], last);
		}
	}
	return 0;
}

static byte MakeSpanishTownName(char *buf, uint32 seed, const char *last)
{
	strecpy(buf, name_spanish_real[SeedChance(0, lengthof(name_spanish_real), seed)], last);
	return 0;
}

static byte MakeFrenchTownName(char *buf, uint32 seed, const char *last)
{
	strecpy(buf, name_french_real[SeedChance(0, lengthof(name_french_real), seed)], last);
	return 0;
}

static byte MakeSillyTownName(char *buf, uint32 seed, const char *last)
{
	strecpy(buf, name_silly_1[SeedChance( 0, lengthof(name_silly_1), seed)], last);
	strecat(buf, name_silly_2[SeedChance(16, lengthof(name_silly_2), seed)], last);
	return 0;
}

static byte MakeSwedishTownName(char *buf, uint32 seed, const char *last)
{
	int i;

	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	/* optional first segment */
	i = SeedChanceBias(0, lengthof(name_swedish_1), seed, 50);
	if (i >= 0)
		strecat(buf, name_swedish_1[i], last);

	/* mandatory middle segments including option of hardcoded name */
	if (SeedChance(4, 5, seed) >= 3) {
		strecat(buf, name_swedish_2[SeedChance( 7, lengthof(name_swedish_2), seed)], last);
	} else {
		strecat(buf, name_swedish_2a[SeedChance( 7, lengthof(name_swedish_2a), seed)], last);
		strecat(buf, name_swedish_2b[SeedChance(10, lengthof(name_swedish_2b), seed)], last);
		strecat(buf, name_swedish_2c[SeedChance(13, lengthof(name_swedish_2c), seed)], last);
	}

	strecat(buf, name_swedish_3[SeedChance(16, lengthof(name_swedish_3), seed)], last);

	return 0;
}

static byte MakeDutchTownName(char *buf, uint32 seed, const char *last)
{
	int i;

	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	/* optional first segment */
	i = SeedChanceBias(0, lengthof(name_dutch_1), seed, 50);
	if (i >= 0)
		strecat(buf, name_dutch_1[i], last);

	/* mandatory middle segments including option of hardcoded name */
	if (SeedChance(6, 9, seed) > 4) {
		strecat(buf, name_dutch_2[SeedChance( 9, lengthof(name_dutch_2), seed)], last);
	} else {
		strecat(buf, name_dutch_3[SeedChance( 9, lengthof(name_dutch_3), seed)], last);
		strecat(buf, name_dutch_4[SeedChance(12, lengthof(name_dutch_4), seed)], last);
	}
	strecat(buf, name_dutch_5[SeedChance(15, lengthof(name_dutch_5), seed)], last);

	return 0;
}

static byte MakeFinnishTownName(char *buf, uint32 seed, const char *last)
{
	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	/* Select randomly if town name should consists of one or two parts. */
	if (SeedChance(0, 15, seed) >= 10) {
		strecat(buf, name_finnish_real[SeedChance(2, lengthof(name_finnish_real), seed)], last);
	} else if (SeedChance(0, 15, seed) >= 5) {
		/* A two-part name by combining one of name_finnish_1 + "la"/"lä"
		 * The reason for not having the contents of name_finnish_{1,2} in the same table is
		 * that the ones in name_finnish_2 are not good for this purpose. */
		uint sel = SeedChance( 0, lengthof(name_finnish_1), seed);
		char *end;
		strecat(buf, name_finnish_1[sel], last);
		end = &buf[strlen(buf)-1];
		if (*end == 'i')
			*end = 'e';
		if (strstr(buf, "a") || strstr(buf, "o") || strstr(buf, "u") ||
			strstr(buf, "A") || strstr(buf, "O") || strstr(buf, "U"))
		{
			strecat(buf, "la", last);
		} else {
			strecat(buf, "lä", last);
		}
	} else {
		/* A two-part name by combining one of name_finnish_{1,2} + name_finnish_3.
		 * Why aren't name_finnish_{1,2} just one table? See above. */
		uint sel = SeedChance(2,
			lengthof(name_finnish_1) + lengthof(name_finnish_2), seed);
		if (sel >= lengthof(name_finnish_1)) {
			strecat(buf, name_finnish_2[sel - lengthof(name_finnish_1)], last);
		} else {
			strecat(buf, name_finnish_1[sel], last);
		}
		strecat(buf, name_finnish_3[SeedChance(10, lengthof(name_finnish_3), seed)], last);
	}

	return 0;
}

static byte MakePolishTownName(char *buf, uint32 seed, const char *last)
{
	uint i;
	uint j;

	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	/* optional first segment */
	i = SeedChance(0,
		lengthof(name_polish_2_o) + lengthof(name_polish_2_m) +
		lengthof(name_polish_2_f) + lengthof(name_polish_2_n),
		seed);
	j = SeedChance(2, 20, seed);


	if (i < lengthof(name_polish_2_o)) {
		strecat(buf, name_polish_2_o[SeedChance(3, lengthof(name_polish_2_o), seed)], last);
	} else if (i < lengthof(name_polish_2_m) + lengthof(name_polish_2_o)) {
		if (j < 4)
			strecat(buf, name_polish_1_m[SeedChance(5, lengthof(name_polish_1_m), seed)], last);

		strecat(buf, name_polish_2_m[SeedChance(7, lengthof(name_polish_2_m), seed)], last);

		if (j >= 4 && j < 16)
			strecat(buf, name_polish_3_m[SeedChance(10, lengthof(name_polish_3_m), seed)], last);
	} else if (i < lengthof(name_polish_2_f) + lengthof(name_polish_2_m) + lengthof(name_polish_2_o)) {
		if (j < 4)
			strecat(buf, name_polish_1_f[SeedChance(5, lengthof(name_polish_1_f), seed)], last);

		strecat(buf, name_polish_2_f[SeedChance(7, lengthof(name_polish_2_f), seed)], last);

		if (j >= 4 && j < 16)
			strecat(buf, name_polish_3_f[SeedChance(10, lengthof(name_polish_3_f), seed)], last);
	} else {
		if (j < 4)
			strecat(buf, name_polish_1_n[SeedChance(5, lengthof(name_polish_1_n), seed)], last);

		strecat(buf, name_polish_2_n[SeedChance(7, lengthof(name_polish_2_n), seed)], last);

		if (j >= 4 && j < 16)
			strecat(buf, name_polish_3_n[SeedChance(10, lengthof(name_polish_3_n), seed)], last);
	}
	return 0;
}

static byte MakeCzechTownName(char *buf, uint32 seed, const char *last)
{
	/* Probability of prefixes/suffixes
	 * 0..11 prefix, 12..13 prefix+suffix, 14..17 suffix, 18..31 nothing */
	int prob_tails;
	bool do_prefix, do_suffix, dynamic_subst;
	/* IDs of the respective parts */
	int prefix = 0, ending = 0, suffix = 0;
	uint postfix = 0;
	uint stem;
	/* The select criteria. */
	CzechGender gender;
	CzechChoose choose;
	CzechAllow allow;

	/* 1:3 chance to use a real name. */
	if (SeedModChance(0, 4, seed) == 0) {
		strecpy(buf, name_czech_real[SeedModChance(4, lengthof(name_czech_real), seed)], last);
		return 0;
	}

	/* NUL terminates the string for strcat() */
	strecpy(buf, "", last);

	prob_tails = SeedModChance(2, 32, seed);
	do_prefix = prob_tails < 12;
	do_suffix = prob_tails > 11 && prob_tails < 17;

	if (do_prefix) prefix = SeedModChance(5, lengthof(name_czech_adj) * 12, seed) / 12;
	if (do_suffix) suffix = SeedModChance(7, lengthof(name_czech_suffix), seed);
	/* 3:1 chance 3:1 to use dynamic substantive */
	stem = SeedModChance(9,
		lengthof(name_czech_subst_full) + 3 * lengthof(name_czech_subst_stem),
		seed);
	if (stem < lengthof(name_czech_subst_full)) {
		/* That was easy! */
		dynamic_subst = false;
		gender = name_czech_subst_full[stem].gender;
		choose = name_czech_subst_full[stem].choose;
		allow = name_czech_subst_full[stem].allow;
	} else {
		unsigned int map[lengthof(name_czech_subst_ending)];
		int ending_start = -1, ending_stop = -1;
		int i;

		/* Load the substantive */
		dynamic_subst = true;
		stem -= lengthof(name_czech_subst_full);
		stem %= lengthof(name_czech_subst_stem);
		gender = name_czech_subst_stem[stem].gender;
		choose = name_czech_subst_stem[stem].choose;
		allow = name_czech_subst_stem[stem].allow;

		/* Load the postfix (1:1 chance that a postfix will be inserted) */
		postfix = SeedModChance(14, lengthof(name_czech_subst_postfix) * 2, seed);

		if (choose & CZC_POSTFIX) {
			/* Always get a real postfix. */
			postfix %= lengthof(name_czech_subst_postfix);
		}
		if (choose & CZC_NOPOSTFIX) {
			/* Always drop a postfix. */
			postfix += lengthof(name_czech_subst_postfix);
		}
		if (postfix < lengthof(name_czech_subst_postfix)) {
			choose |= CZC_POSTFIX;
		} else {
			choose |= CZC_NOPOSTFIX;
		}

		/* Localize the array segment containing a good gender */
		for (ending = 0; ending < (int) lengthof(name_czech_subst_ending); ending++) {
			const CzechNameSubst *e = &name_czech_subst_ending[ending];

			if (gender == CZG_FREE ||
					(gender == CZG_NFREE && e->gender != CZG_SNEUT && e->gender != CZG_PNEUT) ||
					gender == e->gender) {
				if (ending_start < 0)
					ending_start = ending;

			} else if (ending_start >= 0) {
				ending_stop = ending - 1;
				break;
			}
		}
		if (ending_stop < 0) {
			/* Whoa. All the endings matched. */
			ending_stop = ending - 1;
		}

		/* Make a sequential map of the items with good mask */
		i = 0;
		for (ending = ending_start; ending <= ending_stop; ending++) {
			const CzechNameSubst *e = &name_czech_subst_ending[ending];

			if ((e->choose & choose) == choose && (e->allow & allow) != 0)
				map[i++] = ending;
		}
		assert(i > 0);

		/* Load the ending */
		ending = map[SeedModChance(16, i, seed)];
		/* Override possible CZG_*FREE; this must be a real gender,
		 * otherwise we get overflow when modifying the adjectivum. */
		gender = name_czech_subst_ending[ending].gender;
		assert(gender != CZG_FREE && gender != CZG_NFREE);
	}

	if (do_prefix && (name_czech_adj[prefix].choose & choose) != choose) {
		/* Throw away non-matching prefix. */
		do_prefix = false;
	}

	/* Now finally construct the name */

	if (do_prefix) {
		CzechPattern pattern = name_czech_adj[prefix].pattern;
		size_t endpos;

		strecat(buf, name_czech_adj[prefix].name, last);
		endpos = strlen(buf) - 1;
		/* Find the first character in a UTF-8 sequence */
		while (GB(buf[endpos], 6, 2) == 2) endpos--;
		if (gender == CZG_SMASC && pattern == CZP_PRIVL) {
			/* -ovX -> -uv */
			buf[endpos - 2] = 'u';
			assert(buf[endpos - 1] == 'v');
			buf[endpos] = '\0';
		} else {
			strecpy(buf + endpos, name_czech_patmod[gender][pattern], last);
		}

		strecat(buf, " ", last);
	}

	if (dynamic_subst) {
		strecat(buf, name_czech_subst_stem[stem].name, last);
		if (postfix < lengthof(name_czech_subst_postfix)) {
			const char *poststr = name_czech_subst_postfix[postfix];
			const char *endstr = name_czech_subst_ending[ending].name;
			size_t postlen, endlen;

			postlen = strlen(poststr);
			endlen = strlen(endstr);
			assert(postlen > 0 && endlen > 0);

			/* Kill the "avava" and "Jananna"-like cases */
			if (postlen < 2 || postlen > endlen || (
						(poststr[1] != 'v' || poststr[1] != endstr[1]) &&
						poststr[2] != endstr[1])
					) {
				size_t buflen;
				strecat(buf, poststr, last);
				buflen = strlen(buf);

				/* k-i -> c-i, h-i -> z-i */
				if (endstr[0] == 'i') {
					switch (buf[buflen - 1]) {
						case 'k': buf[buflen - 1] = 'c'; break;
						case 'h': buf[buflen - 1] = 'z'; break;
						default: break;
					}
				}
			}
		}
		strecat(buf, name_czech_subst_ending[ending].name, last);
	} else {
		strecat(buf, name_czech_subst_full[stem].name, last);
	}

	if (do_suffix) {
		strecat(buf, " ", last);
		strecat(buf, name_czech_suffix[suffix], last);
	}

	return 0;
}

static byte MakeRomanianTownName(char *buf, uint32 seed, const char *last)
{
	strecpy(buf, name_romanian_real[SeedChance(0, lengthof(name_romanian_real), seed)], last);
	return 0;
}

static byte MakeSlovakTownName(char *buf, uint32 seed, const char *last)
{
	strecpy(buf, name_slovak_real[SeedChance(0, lengthof(name_slovak_real), seed)], last);
	return 0;
}

static byte MakeNorwegianTownName(char *buf, uint32 seed, const char *last)
{
	strecpy(buf, "", last);

	/* Use first 4 bit from seed to decide whether or not this town should
	 * have a real name 3/16 chance.  Bit 0-3 */
	if (SeedChance(0, 15, seed) < 3) {
		/* Use 7bit for the realname table index.  Bit 4-10 */
		strecat(buf, name_norwegian_real[SeedChance(4, lengthof(name_norwegian_real), seed)], last);
	} else {
		/* Use 7bit for the first fake part.  Bit 4-10 */
		strecat(buf, name_norwegian_1[SeedChance(4, lengthof(name_norwegian_1), seed)], last);
		/* Use 7bit for the last fake part.  Bit 11-17 */
		strecat(buf, name_norwegian_2[SeedChance(11, lengthof(name_norwegian_2), seed)], last);
	}

	return 0;
}

static byte MakeHungarianTownName(char *buf, uint32 seed, const char *last)
{
	uint i;

	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	if (SeedChance(12, 15, seed) < 3) {
		strecat(buf, name_hungarian_real[SeedChance(0, lengthof(name_hungarian_real), seed)], last);
	} else {
		/* optional first segment */
		i = SeedChance(3, lengthof(name_hungarian_1) * 3, seed);
		if (i < lengthof(name_hungarian_1))
			strecat(buf, name_hungarian_1[i], last);

		/* mandatory middle segments */
		strecat(buf, name_hungarian_2[SeedChance(3, lengthof(name_hungarian_2), seed)], last);
		strecat(buf, name_hungarian_3[SeedChance(6, lengthof(name_hungarian_3), seed)], last);

		/* optional last segment */
		i = SeedChance(10, lengthof(name_hungarian_4) * 3, seed);
		if (i < lengthof(name_hungarian_4)) {
			strecat(buf, name_hungarian_4[i], last);
		}
	}

	return 0;
}

static byte MakeSwissTownName(char *buf, uint32 seed, const char *last)
{
	strecpy(buf, name_swiss_real[SeedChance(0, lengthof(name_swiss_real), seed)], last);
	return 0;
}

static byte MakeDanishTownName(char *buf, uint32 seed, const char *last)
{
	int i;

	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	/* optional first segment */
	i = SeedChanceBias(0, lengthof(name_danish_1), seed, 50);
	if (i >= 0)
		strecat(buf, name_danish_1[i], last);

	/* middle segments removed as this algorithm seems to create much more realistic names */
	strecat(buf, name_danish_2[SeedChance( 7, lengthof(name_danish_2), seed)], last);
	strecat(buf, name_danish_3[SeedChance(16, lengthof(name_danish_3), seed)], last);

	return 0;
}

static byte MakeTurkishTownName(char *buf, uint32 seed, const char *last)
{
	uint i;

	/* null terminates the string for strcat */
	strecpy(buf, "", last);

	if ((i = SeedModChance(0, 5, seed)) == 0) {
		strecat(buf, name_turkish_prefix[SeedModChance( 2, lengthof(name_turkish_prefix), seed)], last);

		/* middle segment */
		strecat(buf, name_turkish_middle[SeedModChance( 4, lengthof(name_turkish_middle), seed)], last);

		/* optional suffix */
		if (SeedModChance(0, 7, seed) == 0) {
			strecat(buf, name_turkish_suffix[SeedModChance( 10, lengthof(name_turkish_suffix), seed)], last);
		}
	} else {
		if (i == 1 || i == 2) {
			strecat(buf, name_turkish_prefix[SeedModChance( 2, lengthof(name_turkish_prefix), seed)], last);
			strecat(buf, name_turkish_suffix[SeedModChance( 4, lengthof(name_turkish_suffix), seed)], last);
		} else {
			strecat(buf, name_turkish_real[SeedModChance( 4, lengthof(name_turkish_real), seed)], last);
		}
	}
	return 0;
}

static const char *mascul_femin_italian[] = {
	"o",
	"a",
};

static byte MakeItalianTownName(char *buf, uint32 seed, const char *last)
{
	strecpy(buf, "", last);

	if (SeedModChance(0, 6, seed) == 0) { // real city names
		strecat(buf, name_italian_real[SeedModChance(4, lengthof(name_italian_real), seed)], last);
	} else {
		uint i;

		if (SeedModChance(0, 8, seed) == 0) { // prefix
			strecat(buf, name_italian_pref[SeedModChance(11, lengthof(name_italian_pref), seed)], last);
		}

		i = SeedChance(0, 2, seed);
		if (i == 0) { // masculine form
			strecat(buf, name_italian_1m[SeedModChance(4, lengthof(name_italian_1m), seed)], last);
		} else { // feminine form
			strecat(buf, name_italian_1f[SeedModChance(4, lengthof(name_italian_1f), seed)], last);
		}

		if (SeedModChance(3, 3, seed) == 0) {
			strecat(buf, name_italian_2[SeedModChance(11, lengthof(name_italian_2), seed)], last);
			strecat(buf, mascul_femin_italian[i], last);
		} else {
			strecat(buf, name_italian_2i[SeedModChance(16, lengthof(name_italian_2i), seed)], last);
		}

		if (SeedModChance(15, 4, seed) == 0) {
			if (SeedModChance(5, 2, seed) == 0) { // generic suffix
				strecat(buf, name_italian_3[SeedModChance(4, lengthof(name_italian_3), seed)], last);
			} else { // river name suffix
				strecat(buf, name_italian_river1[SeedModChance(4, lengthof(name_italian_river1), seed)], last);
				strecat(buf, name_italian_river2[SeedModChance(16, lengthof(name_italian_river2), seed)], last);
			}
		}
	}

	return 0;
}

static byte MakeCatalanTownName(char *buf, uint32 seed, const char *last)
{
	strecpy(buf, "", last);

	if (SeedModChance(0, 3, seed) == 0) { // real city names
		strecat(buf, name_catalan_real[SeedModChance(4, lengthof(name_catalan_real), seed)], last);
	} else {
		uint i;

		if (SeedModChance(0, 2, seed) == 0) { // prefix
			strecat(buf, name_catalan_pref[SeedModChance(11, lengthof(name_catalan_pref), seed)], last);
		}

		i = SeedChance(0, 2, seed);
		if (i == 0) { // masculine form
			strecat(buf, name_catalan_1m[SeedModChance(4, lengthof(name_catalan_1m), seed)], last);
			strecat(buf, name_catalan_2m[SeedModChance(11, lengthof(name_catalan_2m), seed)], last);
		} else { // feminine form
			strecat(buf, name_catalan_1f[SeedModChance(4, lengthof(name_catalan_1f), seed)], last);
			strecat(buf, name_catalan_2f[SeedModChance(11, lengthof(name_catalan_2f), seed)], last);
		}


		if (SeedModChance(15, 5, seed) == 0) {
			if (SeedModChance(5, 2, seed) == 0) { // generic suffix
				strecat(buf, name_catalan_3[SeedModChance(4, lengthof(name_catalan_3), seed)], last);
			} else { // river name suffix
				strecat(buf, name_catalan_river1[SeedModChance(4, lengthof(name_catalan_river1), seed)], last);
			}
		}
	}

	return 0;
}



TownNameGenerator * const _town_name_generators[] =
{
	MakeEnglishOriginalTownName,
	MakeFrenchTownName,
	MakeGermanTownName,
	MakeEnglishAdditionalTownName,
	MakeSpanishTownName,
	MakeSillyTownName,
	MakeSwedishTownName,
	MakeDutchTownName,
	MakeFinnishTownName,
	MakePolishTownName,
	MakeSlovakTownName,
	MakeNorwegianTownName,
	MakeHungarianTownName,
	MakeAustrianTownName,
	MakeRomanianTownName,
	MakeCzechTownName,
	MakeSwissTownName,
	MakeDanishTownName,
	MakeTurkishTownName,
	MakeItalianTownName,
	MakeCatalanTownName,
};
