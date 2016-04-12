/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file townnamegen.cpp Original town name generators. */

#include "stdafx.h"
#include "townnamegen.h"
#include "core/random_func.hpp"

#include "table/townname.h"


/**
 * Generates a number from given seed.
 * @param shift_by number of bits seed is shifted to the right
 * @param max generated number is in interval 0...max-1
 * @param seed seed
 * @return seed transformed to a number from given range
 */
static inline uint32 SeedChance(byte shift_by, int max, uint32 seed)
{
	return (GB(seed, shift_by, 16) * max) >> 16;
}

/**
 * Generates a number from given seed. Uses different algorithm than SeedChance().
 * @param shift_by number of bits seed is shifted to the right
 * @param max generated number is in interval 0...max-1
 * @param seed seed
 * @return seed transformed to a number from given range
 */
static inline uint32 SeedModChance(byte shift_by, int max, uint32 seed)
{
	/* This actually gives *MUCH* more even distribution of the values
	 * than SeedChance(), which is absolutely horrible in that. If
	 * you do not believe me, try with i.e. the Czech town names,
	 * compare the words (nicely visible on prefixes) generated by
	 * SeedChance() and SeedModChance(). Do not get discouraged by the
	 * never-use-modulo myths, which hold true only for the linear
	 * congruential generators (and Random() isn't such a generator).
	 * --pasky
	 * TODO: Perhaps we should use it for all the name generators? --pasky */
	return (seed >> shift_by) % max;
}


/**
 * Choose a string from a string array.
 * @param strs The string array to choose from.
 * @param seed The random seed.
 * @param shift_by The number of bits that the seed is shifted to the right.
 * @return A random string from the array.
 */
template <uint N>
static inline const char *choose_str (const char * const (&strs) [N],
	uint32 seed, uint shift_by)
{
	return strs [SeedChance (shift_by, N, seed)];
}

/**
 * Choose a string from a string array.
 * @param strs The string array to choose from.
 * @param seed The random seed.
 * @param shift_by The number of bits that the seed is shifted to the right.
 * @return A random string from the array.
 */
template <uint N>
static inline const char *choose_str_mod (const char * const (&strs) [N],
	uint32 seed, uint shift_by)
{
	return strs [SeedModChance (shift_by, N, seed)];
}


/**
 * Optionally append a string from an array to a buffer.
 * @param buf The buffer to append the string to.
 * @param strs The string array to choose from.
 * @param seed The random seed.
 * @param shift_by The number of bits that the seed is shifted to the right.
 * @param threshold The threshold value to actually append a string.
 */
template <uint N>
static inline void append_opt (stringb *buf, const char * const (&strs) [N],
	uint32 seed, uint shift_by, uint threshold)
{
	uint i = SeedChance (shift_by, N + threshold, seed);
	if (i >= threshold) buf->append (strs [i - threshold]);
}


/**
 * Replaces english curses and ugly letter combinations by nicer ones.
 * @param buf buffer with town name
 * @param original English (Original) generator was used
 */
static void ReplaceEnglishWords(char *buf, bool original)
{
	static const uint N = 9;
	static const char org[N + 1][4] = {
		{'C','u','n','t'}, {'S','l','a','g'}, {'S','l','u','t'},
		{'F','a','r','t'}, {'D','r','a','r'}, {'D','r','e','h'},
		{'F','r','a','r'}, {'G','r','a','r'}, {'B','r','a','r'},
		{'W','r','a','r'}
	};
	static const char rep[N + 2][4] = {
		{'E','a','s','t'}, {'P','i','t','s'}, {'E','d','i','n'},
		{'B','o','o','t'}, {'Q','u','a','r'}, {'B','a','s','h'},
		{'S','h','o','r'}, {'A','b','e','r'}, {'O','v','e','r'},
		{'I','n','v','e'}, {'S','t','a','n'}
	};

	assert (strlen(buf) >= 4);

	for (uint i = 0; i < N; i++) {
		if (memcmp (buf, org[i], 4) == 0) {
			memcpy (buf, rep[i], 4);
			return;
		}
	}

	if (memcmp (buf, org[N], 4) == 0) {
		memcpy (buf, rep[original ? N : N + 1], 4);
	}
}

/**
 * Generates English (Original) town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeEnglishOriginalTownName (stringb *buf, uint32 seed)
{
	static const char * const names_1[] = {
		"Great ", "Little ", "New ", "Fort ",
	};

	static const char * const names_2[] = {
		"Wr", "B", "C",  "Ch", "Br", "D", "Dr", "F", "Fr",
		"Fl", "G", "Gr", "H",  "L",  "M", "N",  "P", "Pr",
		"Pl", "R", "S",  "S",  "Sl", "T", "Tr", "W",
	};

	static const char * const names_3[] = {
		"ar", "a", "e", "in", "on", "u", "un", "en",
	};

	static const char * const names_4[] = {
		"n", "ning", "ding", "d", "", "t", "fing",
	};

	static const char * const names_5[] = {
		"ville", "ham",     "field", "ton",    "town",   "bridge",
		"bury",  "wood",    "ford",  "hall",   "ston",   "way",
		"stone", "borough", "ley",   "head",   "bourne", "pool",
		"worth", "hill",    "well",  "hattan", "burg",
	};

	static const char * const names_6[] = {
		"-on-sea", " Bay",  " Market", " Cross", " Bridge",
		" Falls",  " City", " Ridge",  " Springs",
	};

	size_t orig_length = buf->length();

	/* optional first segment */
	append_opt (buf, names_1, seed, 0, 50);

	/* mandatory middle segments */
	buf->append (choose_str (names_2, seed,  4));
	buf->append (choose_str (names_3, seed,  7));
	buf->append (choose_str (names_4, seed, 10));
	buf->append (choose_str (names_5, seed, 13));

	/* optional last segment */
	append_opt (buf, names_6, seed, 15, 60);

	/* Ce, Ci => Ke, Ki */
	if (buf->buffer[orig_length] == 'C' && (buf->buffer[orig_length + 1] == 'e' || buf->buffer[orig_length + 1] == 'i')) {
		buf->buffer[orig_length] = 'K';
	}

	assert (buf->length() - orig_length >= 4);
	ReplaceEnglishWords (&buf->buffer[orig_length], true);
}

/**
 * Generates English (Additional) town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeEnglishAdditionalTownName (stringb *buf, uint32 seed)
{
	size_t orig_length = buf->length();

	/* optional first segment */
	append_opt (buf, _name_additional_english_prefix, seed, 0, 50);

	if (SeedChance(3, 20, seed) >= 14) {
		buf->append (choose_str (_name_additional_english_1a, seed, 6));
	} else {
		buf->append (choose_str (_name_additional_english_1b1, seed, 6));
		buf->append (choose_str (_name_additional_english_1b2, seed, 9));
		if (SeedChance(11, 20, seed) >= 4) {
			buf->append (choose_str (_name_additional_english_1b3a, seed, 12));
		} else {
			buf->append (choose_str (_name_additional_english_1b3b, seed, 12));
		}
	}

	buf->append (choose_str (_name_additional_english_2, seed, 14));

	/* optional last segment */
	append_opt (buf, _name_additional_english_3, seed, 15, 60);

	assert (buf->length() - orig_length >= 4);
	ReplaceEnglishWords (&buf->buffer[orig_length], false);
}


/**
 * Generates Austrian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeAustrianTownName (stringb *buf, uint32 seed)
{
	/* Bad, Maria, Gross, ... */
	append_opt (buf, _name_austrian_a1, seed, 0, 15);

	int j = 0;

	int i = SeedChance(4, 6, seed);
	if (i >= 4) {
		/* Kaisers-kirchen */
		buf->append (choose_str (_name_austrian_a2, seed,  7));
		buf->append (choose_str (_name_austrian_a3, seed, 13));
	} else if (i >= 2) {
		/* St. Johann */
		buf->append (choose_str (_name_austrian_a5, seed, 7));
		buf->append (choose_str (_name_austrian_a6, seed, 9));
		j = 1; // More likely to have a " an der " or " am "
	} else {
		/* Zell */
		buf->append (choose_str (_name_austrian_a4, seed, 7));
	}

	i = SeedChance(1, 6, seed);
	if (i >= 4 - j) {
		/* an der Donau (rivers) */
		buf->append (choose_str (_name_austrian_f1, seed, 4));
		buf->append (choose_str (_name_austrian_f2, seed, 5));
	} else if (i >= 2 - j) {
		/* am Dachstein (mountains) */
		buf->append (choose_str (_name_austrian_b1, seed, 4));
		buf->append (choose_str (_name_austrian_b2, seed, 5));
	}
}


/**
 * Generates German town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeGermanTownName (stringb *buf, uint32 seed)
{
	uint seed_derivative = SeedChance(7, 28, seed);

	/* optional prefix */
	if (seed_derivative == 12 || seed_derivative == 19) {
		buf->append (choose_str (_name_german_pre, seed, 2));
	}

	/* mandatory middle segments including option of hardcoded name */
	uint i = SeedChance(3, lengthof(_name_german_real) + lengthof(_name_german_1), seed);
	if (i < lengthof(_name_german_real)) {
		buf->append (_name_german_real[i]);
	} else {
		buf->append (_name_german_1[i - lengthof(_name_german_real)]);
		buf->append (choose_str (_name_german_2, seed, 5));
	}

	/* optional suffix */
	if (seed_derivative == 24) {
		i = SeedChance(9, lengthof(_name_german_4_an_der) + lengthof(_name_german_4_am), seed);
		if (i < lengthof(_name_german_4_an_der)) {
			buf->append (_name_german_3_an_der[0]);
			buf->append (_name_german_4_an_der[i]);
		} else {
			buf->append (_name_german_3_am[0]);
			buf->append (_name_german_4_am[i - lengthof(_name_german_4_an_der)]);
		}
	}
}


/**
 * Generates Latin-American town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeSpanishTownName (stringb *buf, uint32 seed)
{
	buf->append (choose_str (_name_spanish_real, seed, 0));
}


/**
 * Generates French town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeFrenchTownName (stringb *buf, uint32 seed)
{
	buf->append (choose_str (_name_french_real, seed, 0));
}


/**
 * Generates Silly town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeSillyTownName (stringb *buf, uint32 seed)
{
	buf->append (choose_str (_name_silly_1, seed,  0));
	buf->append (choose_str (_name_silly_2, seed, 16));
}


/**
 * Generates Swedish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeSwedishTownName (stringb *buf, uint32 seed)
{
	/* optional first segment */
	append_opt (buf, _name_swedish_1, seed, 0, 50);

	/* mandatory middle segments including option of hardcoded name */
	if (SeedChance(4, 5, seed) >= 3) {
		buf->append (choose_str (_name_swedish_2, seed, 7));
	} else {
		buf->append (choose_str (_name_swedish_2a, seed,  7));
		buf->append (choose_str (_name_swedish_2b, seed, 10));
		buf->append (choose_str (_name_swedish_2c, seed, 13));
	}

	buf->append (choose_str (_name_swedish_3, seed, 16));
}


/**
 * Generates Dutch town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeDutchTownName (stringb *buf, uint32 seed)
{
	/* optional first segment */
	append_opt (buf, _name_dutch_1, seed, 0, 50);

	/* mandatory middle segments including option of hardcoded name */
	if (SeedChance(6, 9, seed) > 4) {
		buf->append (choose_str (_name_dutch_2, seed,  9));
	} else {
		buf->append (choose_str (_name_dutch_3, seed,  9));
		buf->append (choose_str (_name_dutch_4, seed, 12));
	}

	buf->append (choose_str (_name_dutch_5, seed, 15));
}


/**
 * Generates Finnish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeFinnishTownName (stringb *buf, uint32 seed)
{
	/* Select randomly if town name should consists of one or two parts. */
	if (SeedChance(0, 15, seed) >= 10) {
		buf->append (choose_str (_name_finnish_real, seed, 2));
		return;
	}

	if (SeedChance(0, 15, seed) >= 5) {
		const char *orig = &buf->buffer[buf->length()];

		/* A two-part name by combining one of _name_finnish_1 + "la"/"lä"
		 * The reason for not having the contents of _name_finnish_{1,2} in the same table is
		 * that the ones in _name_finnish_2 are not good for this purpose. */
		buf->append (choose_str (_name_finnish_1, seed, 0));
		assert (!buf->empty());
		char *end = &buf->buffer[buf->length() - 1];
		assert(end >= orig);
		if (*end == 'i') *end = 'e';
		if (strstr(orig, "a") != NULL || strstr(orig, "o") != NULL || strstr(orig, "u") != NULL ||
				strstr(orig, "A") != NULL || strstr(orig, "O") != NULL || strstr(orig, "U")  != NULL) {
			buf->append ("la");
		} else {
			buf->append ("l\xC3\xA4");
		}
		return;
	}

	/* A two-part name by combining one of _name_finnish_{1,2} + _name_finnish_3.
	 * Why aren't _name_finnish_{1,2} just one table? See above. */
	uint sel = SeedChance(2, lengthof(_name_finnish_1) + lengthof(_name_finnish_2), seed);
	if (sel >= lengthof(_name_finnish_1)) {
		buf->append (_name_finnish_2[sel - lengthof(_name_finnish_1)]);
	} else {
		buf->append (_name_finnish_1[sel]);
	}

	buf->append (choose_str (_name_finnish_3, seed, 10));
}


/**
 * Generates Polish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakePolishTownName (stringb *buf, uint32 seed)
{
	/* optional first segment */
	uint i = SeedChance(0,
			lengthof(_name_polish_2_o) + lengthof(_name_polish_2_m) +
			lengthof(_name_polish_2_f) + lengthof(_name_polish_2_n),
			seed);
	uint j = SeedChance(2, 20, seed);


	if (i < lengthof(_name_polish_2_o)) {
		buf->append (choose_str (_name_polish_2_o, seed, 3));
		return;
	}

	if (i < lengthof(_name_polish_2_m) + lengthof(_name_polish_2_o)) {
		if (j < 4) {
			buf->append (choose_str (_name_polish_1_m, seed, 5));
		}

		buf->append (choose_str (_name_polish_2_m, seed, 7));

		if (j >= 4 && j < 16) {
			buf->append (choose_str (_name_polish_3_m, seed, 10));
		}
	}

	if (i < lengthof(_name_polish_2_f) + lengthof(_name_polish_2_m) + lengthof(_name_polish_2_o)) {
		if (j < 4) {
			buf->append (choose_str (_name_polish_1_f, seed, 5));
		}

		buf->append (choose_str (_name_polish_2_f, seed, 7));

		if (j >= 4 && j < 16) {
			buf->append (choose_str (_name_polish_3_f, seed, 10));
		}

		return;
	}

	if (j < 4) {
		buf->append (choose_str (_name_polish_1_n, seed, 5));
	}

	buf->append (choose_str (_name_polish_2_n, seed, 7));

	if (j >= 4 && j < 16) {
		buf->append (choose_str (_name_polish_3_n, seed, 10));
	}
}


/**
 * Generates Czech town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeCzechTownName (stringb *buf, uint32 seed)
{
	/* 1:3 chance to use a real name. */
	if (SeedModChance(0, 4, seed) == 0) {
		buf->append (choose_str_mod (_name_czech_real, seed, 4));
		return;
	}

	/* Probability of prefixes/suffixes
	 * 0..11 prefix, 12..13 prefix+suffix, 14..17 suffix, 18..31 nothing */
	int prob_tails = SeedModChance(2, 32, seed);
	bool do_prefix = prob_tails < 12;
	bool do_suffix = prob_tails > 11 && prob_tails < 17;
	bool dynamic_subst;

	/* IDs of the respective parts */
	int prefix = 0, ending = 0, suffix = 0;
	uint postfix = 0;
	uint stem;

	/* The select criteria. */
	CzechGender gender;
	CzechChoose choose;
	CzechAllow allow;

	if (do_prefix) prefix = SeedModChance(5, lengthof(_name_czech_adj) * 12, seed) / 12;
	if (do_suffix) suffix = SeedModChance(7, lengthof(_name_czech_suffix), seed);
	/* 3:1 chance 3:1 to use dynamic substantive */
	stem = SeedModChance(9,
			lengthof(_name_czech_subst_full) + 3 * lengthof(_name_czech_subst_stem),
			seed);
	if (stem < lengthof(_name_czech_subst_full)) {
		/* That was easy! */
		dynamic_subst = false;
		gender = _name_czech_subst_full[stem].gender;
		choose = _name_czech_subst_full[stem].choose;
		allow = _name_czech_subst_full[stem].allow;
	} else {
		unsigned int map[lengthof(_name_czech_subst_ending)];
		int ending_start = -1, ending_stop = -1;

		/* Load the substantive */
		dynamic_subst = true;
		stem -= lengthof(_name_czech_subst_full);
		stem %= lengthof(_name_czech_subst_stem);
		gender = _name_czech_subst_stem[stem].gender;
		choose = _name_czech_subst_stem[stem].choose;
		allow = _name_czech_subst_stem[stem].allow;

		/* Load the postfix (1:1 chance that a postfix will be inserted) */
		postfix = SeedModChance(14, lengthof(_name_czech_subst_postfix) * 2, seed);

		if (choose & CZC_POSTFIX) {
			/* Always get a real postfix. */
			postfix %= lengthof(_name_czech_subst_postfix);
		}
		if (choose & CZC_NOPOSTFIX) {
			/* Always drop a postfix. */
			postfix += lengthof(_name_czech_subst_postfix);
		}
		if (postfix < lengthof(_name_czech_subst_postfix)) {
			choose |= CZC_POSTFIX;
		} else {
			choose |= CZC_NOPOSTFIX;
		}

		/* Localize the array segment containing a good gender */
		for (ending = 0; ending < (int)lengthof(_name_czech_subst_ending); ending++) {
			const CzechNameSubst *e = &_name_czech_subst_ending[ending];

			if (gender == CZG_FREE ||
					(gender == CZG_NFREE && e->gender != CZG_SNEUT && e->gender != CZG_PNEUT) ||
					 gender == e->gender) {
				if (ending_start < 0) {
					ending_start = ending;
				}
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
		size_t i = 0;
		for (ending = ending_start; ending <= ending_stop; ending++) {
			const CzechNameSubst *e = &_name_czech_subst_ending[ending];

			if ((e->choose & choose) == choose && (e->allow & allow) != 0) {
				map[i++] = ending;
			}
		}
		assert(i > 0);

		/* Load the ending */
		ending = map[SeedModChance(16, (int)i, seed)];
		/* Override possible CZG_*FREE; this must be a real gender,
		 * otherwise we get overflow when modifying the adjectivum. */
		gender = _name_czech_subst_ending[ending].gender;
		assert(gender != CZG_FREE && gender != CZG_NFREE);
	}

	if (do_prefix && (_name_czech_adj[prefix].choose & choose) != choose) {
		/* Throw away non-matching prefix. */
		do_prefix = false;
	}

	/* Now finally construct the name */
	if (do_prefix) {
		size_t orig_length = buf->length();

		CzechPattern pattern = _name_czech_adj[prefix].pattern;

		buf->append (_name_czech_adj[prefix].name);

		assert (!buf->empty());
		size_t end_length = buf->length() - 1;
		/* Find the first character in a UTF-8 sequence */
		while (GB(buf->buffer[end_length], 6, 2) == 2) end_length--;

		if (gender == CZG_SMASC && pattern == CZP_PRIVL) {
			assert (end_length >= orig_length + 2);
			/* -ovX -> -uv */
			buf->buffer[end_length - 2] = 'u';
			assert(buf->buffer[end_length - 1] == 'v');
			buf->truncate (end_length);
		} else {
			assert (end_length >= orig_length);
			buf->append (_name_czech_patmod[gender][pattern]);
		}

		buf->append (' ');
	}

	if (dynamic_subst) {
		buf->append (_name_czech_subst_stem[stem].name);
		if (postfix < lengthof(_name_czech_subst_postfix)) {
			const char *poststr = _name_czech_subst_postfix[postfix];
			const char *endstr = _name_czech_subst_ending[ending].name;

			size_t postlen = strlen(poststr);
			size_t endlen = strlen(endstr);
			assert(postlen > 0 && endlen > 0);

			/* Kill the "avava" and "Jananna"-like cases */
			if (postlen < 2 || postlen > endlen ||
					((poststr[1] != 'v' || poststr[1] != endstr[1]) &&
					poststr[2] != endstr[1])) {
				buf->append (poststr);

				/* k-i -> c-i, h-i -> z-i */
				if (endstr[0] == 'i') {
					assert (!buf->empty());
					char *last = &buf->buffer[buf->length() - 1];
					switch (*last) {
						case 'k': *last = 'c'; break;
						case 'h': *last = 'z'; break;
						default: break;
					}
				}
			}
		}
		buf->append (_name_czech_subst_ending[ending].name);
	} else {
		buf->append (_name_czech_subst_full[stem].name);
	}

	if (do_suffix) {
		buf->append (' ');
		buf->append (_name_czech_suffix[suffix]);
	}
}


/**
 * Generates Romanian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeRomanianTownName (stringb *buf, uint32 seed)
{
	buf->append (choose_str (_name_romanian_real, seed, 0));
}


/**
 * Generates Slovak town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeSlovakTownName (stringb *buf, uint32 seed)
{
	buf->append (choose_str (_name_slovak_real, seed, 0));
}


/**
 * Generates Norwegian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeNorwegianTownName (stringb *buf, uint32 seed)
{
	/* Use first 4 bit from seed to decide whether or not this town should
	 * have a real name 3/16 chance.  Bit 0-3 */
	if (SeedChance(0, 15, seed) < 3) {
		/* Use 7bit for the realname table index.  Bit 4-10 */
		buf->append (choose_str (_name_norwegian_real, seed, 4));
		return;
	}

	/* Use 7bit for the first fake part.  Bit 4-10 */
	buf->append (choose_str (_name_norwegian_1, seed,  4));
	/* Use 7bit for the last fake part.  Bit 11-17 */
	buf->append (choose_str (_name_norwegian_2, seed, 11));
}


/**
 * Generates Hungarian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeHungarianTownName (stringb *buf, uint32 seed)
{
	if (SeedChance(12, 15, seed) < 3) {
		buf->append (choose_str (_name_hungarian_real, seed, 0));
		return;
	}

	/* optional first segment */
	uint i = SeedChance(3, lengthof(_name_hungarian_1) * 3, seed);
	if (i < lengthof(_name_hungarian_1)) buf->append (_name_hungarian_1[i]);

	/* mandatory middle segments */
	buf->append (choose_str (_name_hungarian_2, seed, 3));
	buf->append (choose_str (_name_hungarian_3, seed, 6));

	/* optional last segment */
	i = SeedChance(10, lengthof(_name_hungarian_4) * 3, seed);
	if (i < lengthof(_name_hungarian_4)) {
		buf->append (_name_hungarian_4[i]);
	}
}


/**
 * Generates Swiss town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeSwissTownName (stringb *buf, uint32 seed)
{
	buf->append (choose_str (_name_swiss_real, seed, 0));
}


/**
 * Generates Danish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeDanishTownName (stringb *buf, uint32 seed)
{
	/* optional first segment */
	append_opt (buf, _name_danish_1, seed, 0, 50);

	/* middle segments removed as this algorithm seems to create much more realistic names */
	buf->append (choose_str (_name_danish_2, seed,  7));
	buf->append (choose_str (_name_danish_3, seed, 16));
}


/**
 * Generates Turkish town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeTurkishTownName (stringb *buf, uint32 seed)
{
	uint i = SeedModChance(0, 5, seed);

	switch (i) {
		case 0:
			buf->append (choose_str_mod (_name_turkish_prefix, seed, 2));

			/* middle segment */
			buf->append (choose_str_mod (_name_turkish_middle, seed, 4));

			/* optional suffix */
			if (SeedModChance(0, 7, seed) == 0) {
				buf->append (choose_str_mod (_name_turkish_suffix, seed, 10));
			}
			break;

		case 1: case 2:
			buf->append (choose_str_mod (_name_turkish_prefix, seed, 2));
			buf->append (choose_str_mod (_name_turkish_suffix, seed, 4));
			break;

		default:
			buf->append (choose_str_mod (_name_turkish_real, seed, 4));
			break;
	}
}


/**
 * Generates Italian town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeItalianTownName (stringb *buf, uint32 seed)
{
	if (SeedModChance(0, 6, seed) == 0) { // real city names
		buf->append (choose_str_mod (_name_italian_real, seed, 4));
		return;
	}

	static const char mascul_femin_italian[2] = { 'o', 'a' };

	if (SeedModChance(0, 8, seed) == 0) { // prefix
		buf->append (choose_str_mod (_name_italian_pref, seed, 11));
	}

	uint i = SeedChance(0, 2, seed);
	if (i == 0) { // masculine form
		buf->append (choose_str_mod (_name_italian_1m, seed, 4));
	} else { // feminine form
		buf->append (choose_str_mod (_name_italian_1f, seed, 4));
	}

	if (SeedModChance(3, 3, seed) == 0) {
		buf->append (choose_str_mod (_name_italian_2, seed, 11));
		buf->append (mascul_femin_italian[i]);
	} else {
		buf->append (choose_str_mod (_name_italian_2i, seed, 16));
	}

	if (SeedModChance(15, 4, seed) == 0) {
		if (SeedModChance(5, 2, seed) == 0) { // generic suffix
			buf->append (choose_str_mod (_name_italian_3, seed, 4));
		} else { // river name suffix
			buf->append (choose_str_mod (_name_italian_river1, seed,  4));
			buf->append (choose_str_mod (_name_italian_river2, seed, 16));
		}
	}
}


/**
 * Generates Catalan town name from given seed.
 * @param buf output buffer
 * @param seed town name seed
 */
static void MakeCatalanTownName (stringb *buf, uint32 seed)
{
	if (SeedModChance(0, 3, seed) == 0) { // real city names
		buf->append (choose_str_mod (_name_catalan_real, seed, 4));
		return;
	}

	if (SeedModChance(0, 2, seed) == 0) { // prefix
		buf->append (choose_str_mod (_name_catalan_pref, seed, 11));
	}

	uint i = SeedChance(0, 2, seed);
	if (i == 0) { // masculine form
		buf->append (choose_str_mod (_name_catalan_1m, seed,  4));
		buf->append (choose_str_mod (_name_catalan_2m, seed, 11));
	} else { // feminine form
		buf->append (choose_str_mod (_name_catalan_1f, seed,  4));
		buf->append (choose_str_mod (_name_catalan_2f, seed, 11));
	}

	if (SeedModChance(15, 5, seed) == 0) {
		if (SeedModChance(5, 2, seed) == 0) { // generic suffix
			buf->append (choose_str_mod (_name_catalan_3, seed, 4));
		} else { // river name suffix
			buf->append (choose_str_mod (_name_catalan_river1, seed, 4));
		}
	}
}


/**
 * Type for all town name generator functions.
 * @param buf  The buffer to write the name to.
 * @param seed The seed of the town name.
 */
typedef void TownNameGenerator (stringb *buf, uint32 seed);

/** Contains pointer to generator and minimum buffer size (not incl. terminating '\0') */
struct TownNameGeneratorParams {
	byte min; ///< minimum number of characters that need to be printed for generator to work correctly
	TownNameGenerator *proc; ///< generator itself
};

/** Town name generators */
static const TownNameGeneratorParams _town_name_generators[] = {
	{  4, MakeEnglishOriginalTownName},  // replaces first 4 characters of name
	{  0, MakeFrenchTownName},
	{  0, MakeGermanTownName},
	{  4, MakeEnglishAdditionalTownName}, // replaces first 4 characters of name
	{  0, MakeSpanishTownName},
	{  0, MakeSillyTownName},
	{  0, MakeSwedishTownName},
	{  0, MakeDutchTownName},
	{  8, MakeFinnishTownName}, // _name_finnish_1
	{  0, MakePolishTownName},
	{  0, MakeSlovakTownName},
	{  0, MakeNorwegianTownName},
	{  0, MakeHungarianTownName},
	{  0, MakeAustrianTownName},
	{  0, MakeRomanianTownName},
	{ 28, MakeCzechTownName}, // _name_czech_adj + _name_czech_patmod + 1 + _name_czech_subst_stem + _name_czech_subst_postfix
	{  0, MakeSwissTownName},
	{  0, MakeDanishTownName},
	{  0, MakeTurkishTownName},
	{  0, MakeItalianTownName},
	{  0, MakeCatalanTownName},
};


/**
 * Generates town name from given seed. a language.
 * @param buf output buffer
 * @param lang town name language
 * @param seed generation seed
 */
void GenerateTownNameString (stringb *buf, size_t lang, uint32 seed)
{
	assert(lang < lengthof(_town_name_generators));

	/* Some generators need at least 9 bytes in buffer.  English generators need 5 for
	 * string replacing, others use constructions like strlen(buf)-3 and so on.
	 * Finnish generator needs to fit all strings from _name_finnish_1.
	 * Czech generator needs to fit almost whole town name...
	 * These would break. Using another temporary buffer results in ~40% slower code,
	 * so use it only when really needed. */
	const TownNameGeneratorParams *par = &_town_name_generators[lang];
	if (buf->capacity > par->min) {
		par->proc (buf, seed);
		return;
	}

	char *buffer = AllocaM(char, par->min + 1);
	stringb tmp (par->min + 1, buffer);
	par->proc (&tmp, seed);

	buf->append (buffer);
}
