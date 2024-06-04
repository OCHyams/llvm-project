import sys
import statistics
import Levenshtein
#import strsimpy - too slow
import textdistance


if len(sys.argv) != 3:
    print('step-distance.py path1 path2')
    exit(1)

# function: {lines}
fns1 = {}
with open(sys.argv[1]) as file:
    fn = ''
    for line in file:
        if line.startswith('FUNCTION:'):
            fn = line.rstrip()
            # Just reset it if we find the same function...
            fns1[fn] = []
        elif line.startswith('STEP:'):
            assert len(fn) != 0
            fns1[fn].append(line.rstrip())
# function: {lines}
fns2 = {}
with open(sys.argv[2]) as file:
    fn = ''
    for line in file:
        if line.startswith('FUNCTION:'):
            fn = line.rstrip()
            # Just reset it if we find the same function...
            fns2[fn] = []
        elif line.startswith('STEP:'):
            assert len(fn) != 0
            fns2[fn].append(line.rstrip())

tups = []
for key, val in fns1.items():
    if key not in fns2:
        #print(f'Fn missing from 2nd: {key}')
        continue
    tups.append((key, val, fns2[key]))

for key, _ in fns2.items():
    if key not in fns1:
        #print(f'Fn missing from 1st: {key}')
        pass

L = [Levenshtein.distance(b, a) for _, a, b in tups]
WL = [Levenshtein.distance(b, a, weights = (1,2,1)) for _, a, b in tups]
DL = [textdistance.DamerauLevenshtein().distance(b, a) for _, a, b in tups]

JW = [Levenshtein.jaro_winkler(b, a) for _, a, b in tups]

LCSeq = [textdistance.LCSSeq().similarity(b, a) for _, a, b in tups]
LCStr = [textdistance.LCSStr().similarity(b, a) for _, a, b in tups]

SW = [textdistance.SmithWaterman().distance(b, a) for _, a, b in tups]

print('step comparisons')
print(f'Levenshtein (ins=1, del=1, sub=1) (0 [same] to inf): {sum(L)}')
print(f'                                               mean: {statistics.mean(L)}')
print(f'Levenshtein (ins=1, del=2, sub=1) (0 [same] to inf): {sum(WL)}')
print(f'                                               mean: {statistics.mean(WL)}')
print(f'Damerau-Levenshtein               (0 [same] to inf): {sum(DL)}')
print(f'                                               mean: {statistics.mean(DL)}')
print(f'Jaro-Winkler mean                   (0 to 1 [same]): {statistics.mean(JW)}')
# Produces large negatives even for same sequence,
# It's either broken or weird :)
print(f'LCSubsequence max                   (higher better): {max(LCSeq)}')
print(f'LCSubstring max                     (higher better): {max(LCStr)}')
print(f'Smith-Waterman                    (0 [same] to inf): {sum(SW)}')
print(f'                                               mean: {statistics.mean(SW)}')

def Steps(i):
  return sum(len(t[i]) for t in tups)
def getline(s):
    # STEP: file:line:col
    parts = s.split(':')
    assert len(parts) == 4
    return int(parts[2])
def ForwardSteps(i):
    r = 0
    prev = 0
    for t in tups:
        for line in t[i]:
            line = getline(line)
            if line > prev:
                r += 1
            prev = line
    return r
def BackwardSteps(i):
    r = 0
    prev = 0
    for t in tups:
        for line in t[i]:
            line = getline(line)
            if line < prev:
                r += 1
            prev = line
    return r
def DirectionChanges(i):
    r = 0
    prevline = 0
    prevdir = 1 # 1 = fwd, -1 = back
    for t in tups:
        for line in t[i]:
            line = getline(line)
            if line > prevline:
                if prevdir != 1:
                    r += 1
                    prevdir = 1
            elif line < prevline:
                if prevdir != -1:
                    r += 1
                    prevdir = -1
            prevline = line
    return r
def both_and_diff(fn):
    a = fn(1)
    b = fn(2)
    return (a, b, b - a)
def printstat(statfn):
    s = both_and_diff(statfn)
    print(f'{statfn.__name__ }:   {s[1]}, {s[0]}   Increased by: {s[2]}')

print('step stats')
printstat(Steps)
# SameLineSteps going to be zero for us
printstat(ForwardSteps)
printstat(BackwardSteps)
printstat(DirectionChanges)

# Costs of turning first -> second.

# Count insert, delete, substituion operations.
## print(f'Levenshtein (ins=1, del=1, sub=1): {Levenshtein.distance(lines2, lines1)}')
## print(f'Levenshtein (ins=1, del=2, sub=1): {Levenshtein.distance(lines2, lines1, weights=(1,2,1))}')
## print(f'Jaro-Winkler:                      {Levenshtein.jaro_winkler(lines2, lines1)}')
## 
## # Levenshtein with adjacent transposition.
## print(f'Damerau-Levenshtein                {textdistance.DamerauLevenshtein().distance(lines2, lines1)}')
## 
## # Smith-Waterman - info?
## # SLOW
## # print(f'Smith-Waterman                     {textdistance.SmithWaterman().distance(lines2, lines1)}')
## 
## # LCS
## print(f'LCSubsequence                      {textdistance.LCSSeq().distance(lines2, lines1)}')
## print(f'LCSubstring                        {textdistance.LCSStr().distance(lines2, lines1)}')
## # LCSubstring plus recursively the number of matching chars in non-matching regions on both sides. 
## print(f'LCSubstring                        {textdistance.RatcliffObershelp().distance(lines2, lines1)}')

exit(0)