#ifndef HAVE_TYPEDEFS
#  define HAVE_TYPEDEFS

#include <stdint.h>
#define FILENAMESIZE 200

typedef struct AddrParMap AddrParMap;
typedef struct Boot Boot;
typedef struct BootChr BootChr;
typedef struct Bounds Bounds;
typedef struct BranchTab BranchTab;
typedef struct Constraint Constraint;
typedef struct Gene Gene;
typedef struct GetLineTok GetLineTok;
typedef struct GPTree GPTree;
typedef struct HashTab HashTab;
typedef struct HashTabSeq HashTabSeq;
typedef struct LblNdx LblNdx;
typedef struct LineReader LineReader;
typedef struct NameList NameList;
typedef struct NodeStore NodeStore;
typedef struct Param Param;
typedef struct ParStore ParStore;
typedef struct Point Point;
typedef struct PointBuff PointBuff;
typedef struct PopNode PopNode;
typedef struct PopNodeTab PopNodeTab;
typedef struct ScrmReader ScrmReader;
typedef struct SimReader SimReader;
typedef struct SimSched SimSched;
typedef struct SampNdx SampNdx;
typedef struct State State;
typedef struct StrDblMap StrDblMap;
typedef struct StrInt StrInt;
typedef struct StrParMap StrParMap;
typedef struct Tokenizer Tokenizer;
typedef struct UINTqueue UINTqueue;
typedef struct DAFReader DAFReader;
typedef struct RAFReader RAFReader;

#if 1
#  define TIPID_SIZE 32
typedef uint32_t tipId_t;
#else
#  define TIPID_SIZE 64
typedef uint64_t tipId_t;
#endif

#define KL_COST 1
#define CHISQR_COST 2
#define SMPLCHISQR_COST 3
#define POISSON_COST 4
#define LNL_COST 5
#define COST KL_COST

#define FREE 1
#define FIXED 2
#define CONSTRAINED 4
#define TWON 8
#define TIME 16
#define MIXFRAC 32
#define ARBITRARY 64

#endif
