/* The 1. Salt -> string containing anything, 25 chars */
#define HUMRE1 "psxmafcosxazpwrgcdpakylgd"

/* The 2. Salt -> string containing anything, 9 chars */
#define HUMRE2 "kfdspaqzc"

/* the 1. Code -> a one byte startup code */
#define CODE1 74

/* the 2. Code -> a one byte startup code */
#define CODE2 66

/* the 1. Salt Offset -> value from 0-24 */
#define SA1 24

/* the 2. Salt Offset -> value from 0-8 */
#define SA2 8

/* the make salt routine */
/* dont wonder about the redundance, its needed to somehow hide the fully salts */

/* salt buffers */

unsigned char slt1[26];
unsigned char slt2[10];

int
makesalt (void)
{
  slt1[0] = HUMRE1[1];
  slt1[1] = HUMRE1[0];
  slt1[2] = HUMRE1[3];
  slt1[3] = HUMRE1[2];
  slt1[4] = HUMRE1[5];
  slt1[5] = HUMRE1[4];
  slt1[6] = HUMRE1[6];
  slt1[7] = HUMRE1[7];
  slt1[8] = HUMRE1[8];
  slt1[9] = HUMRE1[9];
  slt1[10] = HUMRE1[11];
  slt1[11] = HUMRE1[10];
  slt1[12] = HUMRE1[12];
  slt1[13] = HUMRE1[13];
  slt1[14] = HUMRE1[15];
  slt1[15] = HUMRE1[14];
  slt1[16] = HUMRE1[16];
  slt1[17] = HUMRE1[17];
  slt1[18] = HUMRE1[18];
  slt1[19] = HUMRE1[19];
  slt1[20] = HUMRE1[22];
  slt1[21] = HUMRE1[21];
  slt1[22] = HUMRE1[20];
  slt1[23] = HUMRE1[24];
  slt1[24] = HUMRE1[23];
  slt1[25] = 0;
  slt2[0] = HUMRE2[1];
  slt2[1] = HUMRE2[0];
  slt2[2] = HUMRE2[2];
  slt2[3] = HUMRE2[3];
  slt2[4] = HUMRE2[4];
  slt2[5] = HUMRE2[8];
  slt2[6] = HUMRE2[6];
  slt2[7] = HUMRE2[7];
  slt2[8] = HUMRE2[5];
  slt2[9] = 0;
  return 0;
}
