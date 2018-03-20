#!/usr/bin/python
from string import maketrans
import sys
import datetime

# Print usage message and abort
def usage():
    msg = \
        """
usage: axt2raf.py <inputfile>

where <inputfile> is in axt format. Writes to standard output.
"""
    sys.stdout.flush()
    print >> sys.stderr, msg
    exit(1)

# For converting from negative strand to positive strand
nucleotides = "atgc"
complements = "tacg"
trtab = maketrans(nucleotides, complements)

class SortError(Exception):
   """ Exception for unsorted input """
   pass

class Alignment:
    def __init__(self):
        self.initialized = False

    def read(self,infile):
        while True:
            # read until we get a non-blank line
            line=f.readline()
            if line == '':
                self.initialized = False
                return self
            line = line.strip().split()
            if len(line) > 0:
                if line[0] == "#": 
                    continue
                break
        sA=f.readline()
        sB=f.readline()
        if sB == '':
            self.initialized = False # signal end of file
            return self
        sA = sA.strip().lower()
        sB = sB.strip().lower()
        lenA = len(sA)
        lenB = len(sB)

        self.alignment = int(line[0])
        self.chr=line[1]
        self.start = int(line[2]) # start position, seq A
        self.end = int(line[3])+1 # 1 past last position, seq A

        # lengths of seqA and seqB should match
        if lenA != lenB:
            sys.stdout.flush()
            print >> sys.stderr, \
                "length mismatch in alignment %d" \
                % self.alignment
            print >> sys.stderr, "lenA=%d but lenB=%d" \
                % (lenA, lenB)
            exit(1)

        # After omitting gaps, length of seqA should match header
        netA = lenA - sA.count("-")
        if netA != self.end - self.start:
            sys.stdout.flush()
            print >> sys.stderr, \
                "non-gap length mismatch: seqA and header in alignment %d" \
                % self.alignment
            print >> sys.stderr, "header=%d but netA=%d" \
                % (self.end - self.start, netA)
            exit(1)

        # After omitting gaps, length of seqB should match header
        netB = lenB - sB.count("-")
        startB = int(line[5])     # start, seq B
        endB = int(line[6])+1     # end, seq B
        if netB != endB - startB:
            sys.stdout.flush()
            print >> sys.stderr, \
                "non-gap length mismatch: seqB and header in alignment %d" \
                % self.alignment
            print >> sys.stderr, "header=%d but netB=%d" \
                % (endB - startB, netA)
            exit(1)
            
        # If we're on the negative strand, then translate
        # sA and sB by complementing each nucleotide.
        # See definition of trtab above.
        strand = line[7]
        if strand == "-":
            sA = sA.translate(trtab)
            sB = sB.translate(trtab)

        self.qual = int(line[8])

        # netA is length of output vectors
        self.ref = netA * [None]
        self.alt = netA * [None]
        self.raf = netA * [None]

        # i indexes ref, alt, and p
        # j indexes sA and sB
        i = j = 0
        while i < netA:
            if sA[j] != "-":
                self.ref[i] = sA[j]
                if sA[j] == sB[j]:
                    self.alt[i] = "."
                    self.raf[i] = 1.0
                elif sB[j] == "-":
                    self.alt[i] = "-" # deletion in sB
                elif sB[j] == "n":
                    self.alt[i] = sB[j]
                else:
                    self.alt[i] = sB[j]
                    self.raf[i] = 0.0
                i += 1
            j += 1

        self.initialized = True
        return self

    # Print alignment
    def pr(self):
        #print "# Alignment %d: [%d, %d)" % \
        #    (self.alignment, self.start, self.end)
        pos = self.start
        for i in range(len(self.ref)):
            # omit deletions and missing values
            if self.alt[i] not in "-n" and self.ref[i] not in "-n":
                print "%s\t%d\t%s\t%s\t%f" % (self.chr, pos, self.ref[i],\
                                                  self.alt[i], self.raf[i])
            pos = pos + 1
        self.initialized = False
        return self

    # Define "+=" operator, which merges two alignments
    def __iadd__(self, other):
        if not self.initialized:
            raise ValueError, "Alignment not initialized"
        if not other.initialized:
            raise ValueError, "Alignment not initialized"
        if self.chr != other.chr:
            raise ValueError, "Chromosomes don't match"
        if self.start > other.start:
            raise ValueError, "Start position of lhs exceeds rhs"
        if other.start > self.end:
            raise ValueError, "Alignments don't overlap"
        if other.start == self.end:
            self.s += other.s
            self.end = other.end
        if other.end < self.end:
            # other is nested within self: do nothing
            return self
        else:
            n = other.start - self.start
            self.ref = self.ref[0:n] + other.ref
            self.alt = self.alt[0:n] + other.alt
            self.raf = self.raf[0:n] + other.raf
        self.initialized = True
        other.initialized = False
        return self

# Do two alignments overlap?
def overlap(a, b):
    assert a.start <= b.start
    if a.chr == b.chr and b.start < a.end:
        return True
    else:
        return False

if len(sys.argv) != 2:
    usage()

try:
    f=open(sys.argv[1])
except:
    sys.stdout.flush()
    print >> sys.stderr, "Can't open input file \"%s\"" % sys.argv[1]
    exit(1)

a = Alignment()
b = Alignment()

a.read(f)
if a.initialized == False:
    sys.stdout.flush()
    print >> sys.stderr, "Can't read 1st alignment"
    exit(1)

print "#%s\t%s\t%s\t%s\t%s" % ("chr", "pos", "ref", "alt", "raf")
while True:
    b.read(f)
    if b.initialized == False:
        break
    if a.start > b.start:
        sys.stdout.flush()
        print >> sys.stderr, \
            "Start positions missorted: %d > %d" % (a.start, b.start)
        exit(1)
    if overlap(a, b):
        a += b
    else:
        a.pr()
        a, b = b, a # swap a and b

if a.initialized:
    a.pr()

if b.initialized:
    b.pr()