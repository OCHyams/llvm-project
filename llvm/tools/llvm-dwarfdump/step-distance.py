import sys
import Levenshtein
#import strsimpy - too slow
import textdistance

if len(sys.argv) != 3:
    print('step-distance.py path1 path2')
    exit(1)

with open(sys.argv[1]) as file:
    lines1 = [line.rstrip() for line in file if line.startswith('STEP:')]
with open(sys.argv[2]) as file:
    lines2 = [line.rstrip() for line in file if line.startswith('STEP:')]

# Costs of turning first -> second.

# Count insert, delete, substituion operations.
print(f'Levenshtein (ins=1, del=1, sub=1): {Levenshtein.distance(lines2, lines1)}')
print(f'Levenshtein (ins=1, del=2, sub=1): {Levenshtein.distance(lines2, lines1, weights=(1,2,1))}')
print(f'Jaro-Winkler:                      {Levenshtein.jaro_winkler(lines2, lines1)}')

# Levenshtein with adjacent transposition.
print(f'Damerau-Levenshtein                {textdistance.DamerauLevenshtein().distance(lines2, lines1)}')

# Smith-Waterman - info?
# SLOW
# print(f'Smith-Waterman                     {textdistance.SmithWaterman().distance(lines2, lines1)}')

# LCS
print(f'LCSubsequence                      {textdistance.LCSSeq().distance(lines2, lines1)}')
print(f'LCSubstring                        {textdistance.LCSStr().distance(lines2, lines1)}')
# LCSubstring plus recursively the number of matching chars in non-matching regions on both sides. 
print(f'LCSubstring                        {textdistance.RatcliffObershelp().distance(lines2, lines1)}')

exit(0)