#include <malloc.h>
#include <assert.h>

struct malloc_chunk
{
	INTERNAL_SIZE_T prev_size; /* Size of previous chunk (if free). */
	INTERNAL_SIZE_T size;      /* Size in bytes, including overhead. */
	struct malloc_chunk* fd;   /* double links -- used only if free. */
	struct malloc_chunk* bk;
} __attribute__((__may_alias__)) ;

typedef struct malloc_chunk* mchunkptr;


/*  sizes, alignments */
#define SIZE_SZ                (sizeof(INTERNAL_SIZE_T))
#define MALLOC_ALIGNMENT       (SIZE_SZ + SIZE_SZ)
#define MALLOC_ALIGN_MASK      (MALLOC_ALIGNMENT - 1)
#define MINSIZE                (sizeof(struct malloc_chunk))

/* conversion from malloc headers to user pointers, and back */

#define chunk2mem(p)   ((Void_t*)((char*)(p) + 2*SIZE_SZ))
#define mem2chunk(mem) ((mchunkptr)((char*)(mem) - 2*SIZE_SZ))

/* pad request bytes into a usable size */
#define request2size(req) \
		(((long)((req) + (SIZE_SZ + MALLOC_ALIGN_MASK)) < \
		  (long)(MINSIZE + MALLOC_ALIGN_MASK)) ? MINSIZE : \
		 (((req) + (SIZE_SZ + MALLOC_ALIGN_MASK)) & ~(MALLOC_ALIGN_MASK)))

/* Check if m has acceptable alignment */
#define aligned_OK(m)    (((unsigned long)((m)) & (MALLOC_ALIGN_MASK)) == 0)

/* size field is or'ed with PREV_INUSE when previous adjacent chunk in use */
#define PREV_INUSE 0x1

/* size field is or'ed with IS_MMAPPED if the chunk was obtained with mmap() */
#define IS_MMAPPED 0x2

/* Bits to mask off when extracting size */
#define SIZE_BITS (PREV_INUSE|IS_MMAPPED)

/* Ptr to next physical malloc_chunk. */
#define next_chunk(p) ((mchunkptr)( ((char*)(p)) + ((p)->size & ~PREV_INUSE) ))

/* Ptr to previous physical malloc_chunk */
#define prev_chunk(p)\
		((mchunkptr)( ((char*)(p)) - ((p)->prev_size) ))

/* Treat space at ptr + offset as a chunk */
#define chunk_at_offset(p, s)  ((mchunkptr)(((char*)(p)) + (s)))

/* Dealing with use bits */

/* extract p's inuse bit */
#define inuse(p)\
		((((mchunkptr)(((char*)(p))+((p)->size & ~PREV_INUSE)))->size) & PREV_INUSE)

/* extract inuse bit of previous chunk */
#define prev_inuse(p)  ((p)->size & PREV_INUSE)

/* check for mmap()'ed chunk */
#define chunk_is_mmapped(p) ((p)->size & IS_MMAPPED)

/* set/clear chunk as in use without otherwise disturbing */
#define set_inuse(p)\
		((mchunkptr)(((char*)(p)) + ((p)->size & ~PREV_INUSE)))->size |= PREV_INUSE

#define clear_inuse(p)\
		((mchunkptr)(((char*)(p)) + ((p)->size & ~PREV_INUSE)))->size &= ~(PREV_INUSE)

/* check/set/clear inuse bits in known places */
#define inuse_bit_at_offset(p, s)\
		(((mchunkptr)(((char*)(p)) + (s)))->size & PREV_INUSE)

#define set_inuse_bit_at_offset(p, s)\
		(((mchunkptr)(((char*)(p)) + (s)))->size |= PREV_INUSE)

#define clear_inuse_bit_at_offset(p, s)\
		(((mchunkptr)(((char*)(p)) + (s)))->size &= ~(PREV_INUSE))


	/* Dealing with size fields */

/* Get size, ignoring use bits */
#define chunksize(p)          ((p)->size & ~(SIZE_BITS))

/* Set size at head, without disturbing its use bit */
#define set_head_size(p, s)   ((p)->size = (((p)->size & PREV_INUSE) | (s)))

/* Set size/use ignoring previous bits in header */
#define set_head(p, s)        ((p)->size = (s))

/* Set size at footer (only when chunk is not in use) */
#define set_foot(p, s)   (((mchunkptr)((char*)(p) + (s)))->prev_size = (s))

#define NAV             128   /* number of bins */

typedef struct malloc_chunk* mbinptr;

/* access macros */
#define bin_at(i)      ((mbinptr)((char*)&(av_[2*(i) + 2]) - 2*SIZE_SZ))
#define next_bin(b)    ((mbinptr)((char*)(b) + 2 * sizeof(mbinptr)))
#define prev_bin(b)    ((mbinptr)((char*)(b) - 2 * sizeof(mbinptr)))

#define top            (av_[2])          /* The topmost chunk */
#define last_remainder (bin_at(1))       /* remainder from last split */

/*
 * Because top initially points to its own bin with initial
 * zero size, thus forcing extension on the first malloc request,
 * we avoid having any special code in malloc to check whether
 * it even exists yet. But we still need to in malloc_extend_top.
 */
#define initial_top    ((mchunkptr)(bin_at(0)))

/* Helper macro to initialize bins */
#define IAV(i)  bin_at(i), bin_at(i)

static mbinptr av_[NAV * 2 + 2] = {
	0, 0,
	IAV(0),   IAV(1),   IAV(2),   IAV(3),   IAV(4),   IAV(5),   IAV(6),   IAV(7),
	IAV(8),   IAV(9),   IAV(10),  IAV(11),  IAV(12),  IAV(13),  IAV(14),  IAV(15),
	IAV(16),  IAV(17),  IAV(18),  IAV(19),  IAV(20),  IAV(21),  IAV(22),  IAV(23),
	IAV(24),  IAV(25),  IAV(26),  IAV(27),  IAV(28),  IAV(29),  IAV(30),  IAV(31),
	IAV(32),  IAV(33),  IAV(34),  IAV(35),  IAV(36),  IAV(37),  IAV(38),  IAV(39),
	IAV(40),  IAV(41),  IAV(42),  IAV(43),  IAV(44),  IAV(45),  IAV(46),  IAV(47),
	IAV(48),  IAV(49),  IAV(50),  IAV(51),  IAV(52),  IAV(53),  IAV(54),  IAV(55),
	IAV(56),  IAV(57),  IAV(58),  IAV(59),  IAV(60),  IAV(61),  IAV(62),  IAV(63),
	IAV(64),  IAV(65),  IAV(66),  IAV(67),  IAV(68),  IAV(69),  IAV(70),  IAV(71),
	IAV(72),  IAV(73),  IAV(74),  IAV(75),  IAV(76),  IAV(77),  IAV(78),  IAV(79),
	IAV(80),  IAV(81),  IAV(82),  IAV(83),  IAV(84),  IAV(85),  IAV(86),  IAV(87),
	IAV(88),  IAV(89),  IAV(90),  IAV(91),  IAV(92),  IAV(93),  IAV(94),  IAV(95),
	IAV(96),  IAV(97),  IAV(98),  IAV(99),  IAV(100), IAV(101), IAV(102), IAV(103),
	IAV(104), IAV(105), IAV(106), IAV(107), IAV(108), IAV(109), IAV(110), IAV(111),
	IAV(112), IAV(113), IAV(114), IAV(115), IAV(116), IAV(117), IAV(118), IAV(119),
	IAV(120), IAV(121), IAV(122), IAV(123), IAV(124), IAV(125), IAV(126), IAV(127)
};

ulong mem_malloc_start = 0;
ulong mem_malloc_end = 0;
ulong mem_malloc_brk = 0;

void *sbrk(ptrdiff_t increment)
{
	ulong old = mem_malloc_brk;
	ulong new = old + increment;

	/*
	 * if we are giving memory back make sure we clear it out since
	 * we set MORECORE_CLEARS to 1
	 */
	if (increment < 0)
		memset((void *)new, 0, -increment);

	if ((new < mem_malloc_start) || (new > mem_malloc_end))
		return (void *)MORECORE_FAILURE;

	mem_malloc_brk = new;

	return (void *)old;
}

void mem_malloc_init(ulong start, ulong size)
{
	mem_malloc_start = start;
	mem_malloc_end = start + size;
	mem_malloc_brk = start;

	memset((void *)mem_malloc_start, 0, size);
}

/* field-extraction macros */
#define first(b) ((b)->fd)
#define last(b)  ((b)->bk)

/* Indexing into bins */
#define bin_index(sz)                                                          \
	(((((unsigned long)(sz)) >> 9) ==    0) ?       (((unsigned long)(sz)) >>  3): \
	 ((((unsigned long)(sz)) >> 9) <=    4) ?  56 + (((unsigned long)(sz)) >>  6): \
	 ((((unsigned long)(sz)) >> 9) <=   20) ?  91 + (((unsigned long)(sz)) >>  9): \
	 ((((unsigned long)(sz)) >> 9) <=   84) ? 110 + (((unsigned long)(sz)) >> 12): \
	 ((((unsigned long)(sz)) >> 9) <=  340) ? 119 + (((unsigned long)(sz)) >> 15): \
	 ((((unsigned long)(sz)) >> 9) <= 1364) ? 124 + (((unsigned long)(sz)) >> 18): \
	 126)

/*
 * bins for chunks < 512 are all spaced 8 bytes apart, and hold
 * identically sized chunks. This is exploited in malloc.
 */
#define MAX_SMALLBIN         63
#define MAX_SMALLBIN_SIZE   512
#define SMALLBIN_WIDTH        8

#define smallbin_index(sz)  (((unsigned long)(sz)) >> 3)

/* Requests are `small' if both the corresponding and the next bin are small */
#define is_small_request(nb) (nb < MAX_SMALLBIN_SIZE - SMALLBIN_WIDTH)

/*
 * To help compensate for the large number of bins, a one-level index
 * structure is used for bin-by-bin searching.  `binblocks' is a
 * one-word bitvector recording whether groups of BINBLOCKWIDTH bins
 * have any (possibly) non-empty bins, so they can be skipped over
 * all at once during during traversals. The bits are NOT always
 * cleared as soon as all bins in a block are empty, but instead only
 * when all are noticed to be empty during traversal in malloc.
 */
#define BINBLOCKWIDTH     4   /* bins per block */

#define binblocks_r     ((INTERNAL_SIZE_T)av_[1]) /* bitvector of nonempty blocks */
#define binblocks_w     (av_[1])

/* bin<->block macros */
#define idx2binblock(ix)    ((unsigned)1 << (ix / BINBLOCKWIDTH))
#define mark_binblock(ii)   (binblocks_w = (mbinptr)(binblocks_r | idx2binblock(ii)))
#define clear_binblock(ii)  (binblocks_w = (mbinptr)(binblocks_r & ~(idx2binblock(ii))))

/*  Other static bookkeeping data */

/* variables holding tunable values */
static unsigned long trim_threshold   = DEFAULT_TRIM_THRESHOLD;
static unsigned long top_pad          = DEFAULT_TOP_PAD;
static unsigned int  n_mmaps_max      = DEFAULT_MMAP_MAX;
static unsigned long mmap_threshold   = DEFAULT_MMAP_THRESHOLD;

/* The first value returned from sbrk */
static char* sbrk_base = (char*)(-1);

/* The maximum memory obtained from system via sbrk */
static unsigned long max_sbrked_mem = 0;

/* The maximum via either sbrk or mmap */
static unsigned long max_total_mem = 0;

/* internal working copy of mallinfo */
static struct mallinfo current_mallinfo = {  0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/* The total memory obtained from system via sbrk */
#define sbrked_mem  (current_mallinfo.arena)

/* Tracking mmaps */
static unsigned long mmapped_mem = 0;

#define check_free_chunk(P)
#define check_inuse_chunk(P)
#define check_chunk(P)
#define check_malloced_chunk(P,N)

/*
 * Place chunk p of size s in its bin, in size order,
 * putting it ahead of others of same size.
 */
#define frontlink(P, S, IDX, BK, FD)			\
{							\
	if (S < MAX_SMALLBIN_SIZE)			\
	{						\
		IDX = smallbin_index(S);		\
		mark_binblock(IDX);			\
		BK = bin_at(IDX);			\
		FD = BK->fd;				\
		P->bk = BK;				\
		P->fd = FD;				\
		FD->bk = BK->fd = P;			\
	}						\
	else						\
	{						\
		IDX = bin_index(S);			\
		BK = bin_at(IDX);			\
		FD = BK->fd;				\
		if (FD == BK) mark_binblock(IDX);	\
		else					\
		{					\
			while (FD != BK && S < chunksize(FD)) FD = FD->fd;	\
			BK = FD->bk;			\
		}					\
		P->bk = BK;				\
		P->fd = FD;				\
		FD->bk = BK->fd = P;			\
	}						\
}


/* take a chunk off a list */
#define unlink(P, BK, FD)		\
{					\
	BK = P->bk;			\
	FD = P->fd;			\
	FD->bk = BK;			\
	BK->fd = FD;			\
}

/* Place p as the last remainder */
#define link_last_remainder(P)				\
{							\
	last_remainder->fd = last_remainder->bk = P;	\
	P->fd = P->bk = last_remainder;			\
}

/* Clear the last_remainder bin */
#define clear_last_remainder \
	(last_remainder->fd = last_remainder->bk = last_remainder)



/*
   Extend the top-most chunk by obtaining memory from system.
   Main interface to sbrk (but see also malloc_trim).
   */
static void malloc_extend_top(INTERNAL_SIZE_T nb)
{
	char*     brk;                  /* return value from sbrk */
	INTERNAL_SIZE_T front_misalign; /* unusable bytes at front of sbrked space */
	INTERNAL_SIZE_T correction;     /* bytes for 2nd sbrk call */
	char*     new_brk;              /* return of 2nd sbrk call */
	INTERNAL_SIZE_T top_size;       /* new size of top chunk */

	mchunkptr old_top     = top;  /* Record state of old top */
	INTERNAL_SIZE_T old_top_size = chunksize(old_top);
	char*     old_end      = (char*)(chunk_at_offset(old_top, old_top_size));

	/* Pad request with top_pad plus minimal overhead */

	INTERNAL_SIZE_T    sbrk_size     = nb + top_pad + MINSIZE;
	unsigned long pagesz    = malloc_getpagesize;

	/* If not the first time through, round to preserve page boundary */
	/* Otherwise, we need to correct to a page size below anyway. */
	/* (We also correct below if an intervening foreign sbrk call.) */

	if (sbrk_base != (char*)(-1))
		sbrk_size = (sbrk_size + (pagesz - 1)) & ~(pagesz - 1);

	brk = (char*)(MORECORE (sbrk_size));

	/* Fail if sbrk failed or if a foreign sbrk call killed our space */
	if (brk == (char*)(MORECORE_FAILURE) ||
			(brk < old_end && old_top != initial_top))
		return;

	sbrked_mem += sbrk_size;

	if (brk == old_end) /* can just add bytes to current top */
	{
		top_size = sbrk_size + old_top_size;
		set_head(top, top_size | PREV_INUSE);
	}
	else
	{
		if (sbrk_base == (char*)(-1))  /* First time through. Record base */
			sbrk_base = brk;
		else  /* Someone else called sbrk().  Count those bytes as sbrked_mem. */
			sbrked_mem += brk - (char*)old_end;

		/* Guarantee alignment of first new chunk made from this space */
		front_misalign = (unsigned long)chunk2mem(brk) & MALLOC_ALIGN_MASK;
		if (front_misalign > 0)
		{
			correction = (MALLOC_ALIGNMENT) - front_misalign;
			brk += correction;
		}
		else
			correction = 0;

		/* Guarantee the next brk will be at a page boundary */

		correction += ((((unsigned long)(brk + sbrk_size))+(pagesz-1)) &
				~(pagesz - 1)) - ((unsigned long)(brk + sbrk_size));

		/* Allocate correction */
		new_brk = (char*)(MORECORE (correction));
		if (new_brk == (char*)(MORECORE_FAILURE)) return;

		sbrked_mem += correction;

		top = (mchunkptr)brk;
		top_size = new_brk - brk + correction;
		set_head(top, top_size | PREV_INUSE);

		if (old_top != initial_top)
		{

			/* There must have been an intervening foreign sbrk call. */
			/* A double fencepost is necessary to prevent consolidation */

			/* If not enough space to do this, then user did something very wrong */
			if (old_top_size < MINSIZE)
			{
				set_head(top, PREV_INUSE); /* will force null return from malloc */
				return;
			}

			/* Also keep size a multiple of MALLOC_ALIGNMENT */
			old_top_size = (old_top_size - 3*SIZE_SZ) & ~MALLOC_ALIGN_MASK;
			set_head_size(old_top, old_top_size);
			chunk_at_offset(old_top, old_top_size          )->size =
				SIZE_SZ|PREV_INUSE;
			chunk_at_offset(old_top, old_top_size + SIZE_SZ)->size =
				SIZE_SZ|PREV_INUSE;
			/* If possible, release the rest. */
			if (old_top_size >= MINSIZE)
				fREe(chunk2mem(old_top));
		}
	}

	if ((unsigned long)sbrked_mem > (unsigned long)max_sbrked_mem)
		max_sbrked_mem = sbrked_mem;
	if ((unsigned long)(mmapped_mem + sbrked_mem) > (unsigned long)max_total_mem)
		max_total_mem = mmapped_mem + sbrked_mem;

	/* We always land on a page boundary */
	assert(((unsigned long)((char*)top + top_size) & (pagesz - 1)) == 0);
}

/* Main public routines */
/*
   Malloc Algorthim:

   The requested size is first converted into a usable form, `nb'.
   This currently means to add 4 bytes overhead plus possibly more to
   obtain 8-byte alignment and/or to obtain a size of at least
   MINSIZE (currently 16 bytes), the smallest allocatable size.
   (All fits are considered `exact' if they are within MINSIZE bytes.)

   From there, the first successful of the following steps is taken:

   1. The bin corresponding to the request size is scanned, and if
   a chunk of exactly the right size is found, it is taken.

   2. The most recently remaindered chunk is used if it is big
   enough.  This is a form of (roving) first fit, used only in
   the absence of exact fits. Runs of consecutive requests use
   the remainder of the chunk used for the previous such request
   whenever possible. This limited use of a first-fit style
   allocation strategy tends to give contiguous chunks
   coextensive lifetimes, which improves locality and can reduce
   fragmentation in the long run.

   3. Other bins are scanned in increasing size order, using a
   chunk big enough to fulfill the request, and splitting off
   any remainder.  This search is strictly by best-fit; i.e.,
   the smallest (with ties going to approximately the least
   recently used) chunk that fits is selected.

   4. If large enough, the chunk bordering the end of memory
   (`top') is split off. (This use of `top' is in accord with
   the best-fit search rule.  In effect, `top' is treated as
   larger (and thus less well fitting) than any other available
   chunk since it can be extended to be as large as necessary
   (up to system limitations).

   5. If the request size meets the mmap threshold and the
   system supports mmap, and there are few enough currently
   allocated mmapped regions, and a call to mmap succeeds,
   the request is allocated via direct memory mapping.

   6. Otherwise, the top of memory is extended by
   obtaining more space from the system (normally using sbrk,
   but definable to anything else via the MORECORE macro).
   Memory is gathered from the system (in system page-sized
   units) in a way that allows chunks obtained across different
   sbrk calls to be consolidated, but does not require
   contiguous memory. Thus, it should be safe to intersperse
   mallocs with other sbrk calls.


   All allocations are made from the the `lowest' part of any found
   chunk. (The implementation invariant is that prev_inuse is
   always true of any allocated chunk; i.e., that each allocated
   chunk borders either a previously allocated and still in-use chunk,
   or the base of its memory arena.)

*/

Void_t* mALLOc(size_t bytes)
{
	mchunkptr victim;                  /* inspected/selected chunk */
	INTERNAL_SIZE_T victim_size;       /* its size */
	int       idx;                     /* index for bin traversal */
	mbinptr   bin;                     /* associated bin */
	mchunkptr remainder;               /* remainder from a split */
	long      remainder_size;          /* its size */
	int       remainder_index;         /* its bin index */
	unsigned long block;               /* block traverser bit */
	int       startidx;                /* first bin of a traversed block */
	mchunkptr fwd;                     /* misc temp for linking */
	mchunkptr bck;                     /* misc temp for linking */
	mbinptr q;                         /* misc temp */

	INTERNAL_SIZE_T nb;

	/* check if mem_malloc_init() was run */
	if ((mem_malloc_start == 0) && (mem_malloc_end == 0)) {
		/* not initialized yet */
		return 0;
	}

	if ((long)bytes < 0) return 0;

	nb = request2size(bytes);  /* padded request size; */

	/* Check for exact match in a bin */

	if (is_small_request(nb))  /* Faster version for small requests */
	{
		idx = smallbin_index(nb);

		/* No traversal or size check necessary for small bins.  */

		q = bin_at(idx);
		victim = last(q);

		/* Also scan the next one, since it would have a remainder < MINSIZE */
		if (victim == q)
		{
			q = next_bin(q);
			victim = last(q);
		}
		if (victim != q)
		{
			victim_size = chunksize(victim);
			unlink(victim, bck, fwd);
			set_inuse_bit_at_offset(victim, victim_size);
			check_malloced_chunk(victim, nb);
			return chunk2mem(victim);
		}

		idx += 2; /* Set for bin scan below. We've already scanned 2 bins. */

	}
	else
	{
		idx = bin_index(nb);
		bin = bin_at(idx);

		for (victim = last(bin); victim != bin; victim = victim->bk)
		{
			victim_size = chunksize(victim);
			remainder_size = victim_size - nb;

			if (remainder_size >= (long)MINSIZE) /* too big */
			{
				--idx; /* adjust to rescan below after checking last remainder */
				break;
			}

			else if (remainder_size >= 0) /* exact fit */
			{
				unlink(victim, bck, fwd);
				set_inuse_bit_at_offset(victim, victim_size);
				check_malloced_chunk(victim, nb);
				return chunk2mem(victim);
			}
		}

		++idx;
	}

	/* Try to use the last split-off remainder */

	if ( (victim = last_remainder->fd) != last_remainder)
	{
		victim_size = chunksize(victim);
		remainder_size = victim_size - nb;

		if (remainder_size >= (long)MINSIZE) /* re-split */
		{
			remainder = chunk_at_offset(victim, nb);
			set_head(victim, nb | PREV_INUSE);
			link_last_remainder(remainder);
			set_head(remainder, remainder_size | PREV_INUSE);
			set_foot(remainder, remainder_size);
			check_malloced_chunk(victim, nb);
			return chunk2mem(victim);
		}

		clear_last_remainder;

		if (remainder_size >= 0)  /* exhaust */
		{
			set_inuse_bit_at_offset(victim, victim_size);
			check_malloced_chunk(victim, nb);
			return chunk2mem(victim);
		}

		/* Else place in bin */

		frontlink(victim, victim_size, remainder_index, bck, fwd);
	}

	/*
	   If there are any possibly nonempty big-enough blocks,
	   search for best fitting chunk by scanning bins in blockwidth units.
	   */
	if ( (block = idx2binblock(idx)) <= binblocks_r)
	{

		/* Get to the first marked block */

		if ( (block & binblocks_r) == 0)
		{
			/* force to an even block boundary */
			idx = (idx & ~(BINBLOCKWIDTH - 1)) + BINBLOCKWIDTH;
			block <<= 1;
			while ((block & binblocks_r) == 0)
			{
				idx += BINBLOCKWIDTH;
				block <<= 1;
			}
		}

		/* For each possibly nonempty block ... */
		for (;;)
		{
			startidx = idx;          /* (track incomplete blocks) */
			q = bin = bin_at(idx);

			/* For each bin in this block ... */
			do
			{
				/* Find and use first big enough chunk ... */

				for (victim = last(bin); victim != bin; victim = victim->bk)
				{
					victim_size = chunksize(victim);
					remainder_size = victim_size - nb;

					if (remainder_size >= (long)MINSIZE) /* split */
					{
						remainder = chunk_at_offset(victim, nb);
						set_head(victim, nb | PREV_INUSE);
						unlink(victim, bck, fwd);
						link_last_remainder(remainder);
						set_head(remainder, remainder_size | PREV_INUSE);
						set_foot(remainder, remainder_size);
						check_malloced_chunk(victim, nb);
						return chunk2mem(victim);
					}

					else if (remainder_size >= 0)  /* take */
					{
						set_inuse_bit_at_offset(victim, victim_size);
						unlink(victim, bck, fwd);
						check_malloced_chunk(victim, nb);
						return chunk2mem(victim);
					}

				}

				bin = next_bin(bin);

			} while ((++idx & (BINBLOCKWIDTH - 1)) != 0);

			/* Clear out the block bit. */

			do   /* Possibly backtrack to try to clear a partial block */
			{
				if ((startidx & (BINBLOCKWIDTH - 1)) == 0)
				{
					av_[1] = (mbinptr)(binblocks_r & ~block);
					break;
				}
				--startidx;
				q = prev_bin(q);
			} while (first(q) == q);

			/* Get to the next possibly nonempty block */

			if ( (block <<= 1) <= binblocks_r && (block != 0) )
			{
				while ((block & binblocks_r) == 0)
				{
					idx += BINBLOCKWIDTH;
					block <<= 1;
				}
			}
			else
				break;
		}
	}


	/* Try to use top chunk */

	/* Require that there be a remainder, ensuring top always exists  */
	if ( (remainder_size = chunksize(top) - nb) < (long)MINSIZE)
	{
		/* Try to extend */
		malloc_extend_top(nb);
		if ( (remainder_size = chunksize(top) - nb) < (long)MINSIZE)
			return 0; /* propagate failure */
	}

	victim = top;
	set_head(victim, nb | PREV_INUSE);
	top = chunk_at_offset(victim, nb);
	set_head(top, remainder_size | PREV_INUSE);
	check_malloced_chunk(victim, nb);
	return chunk2mem(victim);

}

/*

   free() algorithm :

   cases:

   1. free(0) has no effect.

   2. If the chunk was allocated via mmap, it is release via munmap().

   3. If a returned chunk borders the current high end of memory,
   it is consolidated into the top, and if the total unused
   topmost memory exceeds the trim threshold, malloc_trim is
   called.

   4. Other chunks are consolidated as they arrive, and
   placed in corresponding bins. (This includes the case of
   consolidating with the current `last_remainder').

*/
void fREe(Void_t* mem)
{
	mchunkptr p;         /* chunk corresponding to mem */
	INTERNAL_SIZE_T hd;  /* its head field */
	INTERNAL_SIZE_T sz;  /* its size */
	int       idx;       /* its bin index */
	mchunkptr next;      /* next contiguous chunk */
	INTERNAL_SIZE_T nextsz; /* its size */
	INTERNAL_SIZE_T prevsz; /* size of previous contiguous chunk */
	mchunkptr bck;       /* misc temp for linking */
	mchunkptr fwd;       /* misc temp for linking */
	int       islr;      /* track whether merging with last_remainder */

	if (mem == 0)                              /* free(0) has no effect */
		return;

	p = mem2chunk(mem);
	hd = p->size;

	check_inuse_chunk(p);

	sz = hd & ~PREV_INUSE;
	next = chunk_at_offset(p, sz);
	nextsz = chunksize(next);

	if (next == top)                            /* merge with top */
	{
		sz += nextsz;

		if (!(hd & PREV_INUSE))                    /* consolidate backward */
		{
			prevsz = p->prev_size;
			p = chunk_at_offset(p, -((long) prevsz));
			sz += prevsz;
			unlink(p, bck, fwd);
		}

		set_head(p, sz | PREV_INUSE);
		top = p;
		if ((unsigned long)(sz) >= (unsigned long)trim_threshold)
			malloc_trim(top_pad);
		return;
	}

	set_head(next, nextsz);                    /* clear inuse bit */

	islr = 0;

	if (!(hd & PREV_INUSE))                    /* consolidate backward */
	{
		prevsz = p->prev_size;
		p = chunk_at_offset(p, -((long) prevsz));
		sz += prevsz;

		if (p->fd == last_remainder)             /* keep as last_remainder */
			islr = 1;
		else
			unlink(p, bck, fwd);
	}

	if (!(inuse_bit_at_offset(next, nextsz)))   /* consolidate forward */
	{
		sz += nextsz;

		if (!islr && next->fd == last_remainder)  /* re-insert last_remainder */
		{
			islr = 1;
			link_last_remainder(p);
		}
		else
			unlink(next, bck, fwd);
	}


	set_head(p, sz | PREV_INUSE);
	set_foot(p, sz);
	if (!islr)
		frontlink(p, sz, idx, bck, fwd);
}

/*

   Realloc algorithm:

   Chunks that were obtained via mmap cannot be extended or shrunk
   unless HAVE_MREMAP is defined, in which case mremap is used.
   Otherwise, if their reallocation is for additional space, they are
   copied.  If for less, they are just left alone.

   Otherwise, if the reallocation is for additional space, and the
   chunk can be extended, it is, else a malloc-copy-free sequence is
   taken.  There are several different ways that a chunk could be
   extended. All are tried:

 * Extending forward into following adjacent free chunk.
 * Shifting backwards, joining preceding adjacent space
 * Both shifting backwards and extending forward.
 * Extending into newly sbrked space

 Unless the #define REALLOC_ZERO_BYTES_FREES is set, realloc with a
 size argument of zero (re)allocates a minimum-sized chunk.

 If the reallocation is for less space, and the new request is for
 a `small' (<512 bytes) size, then the newly unused space is lopped
 off and freed.

 The old unix realloc convention of allowing the last-free'd chunk
 to be used as an argument to realloc is no longer supported.
 I don't know of any programs still relying on this feature,
 and allowing it would also allow too many other incorrect
 usages of realloc to be sensible.


*/


Void_t* rEALLOc(Void_t* oldmem, size_t bytes)
{
	INTERNAL_SIZE_T    nb;      /* padded request size */

	mchunkptr oldp;             /* chunk corresponding to oldmem */
	INTERNAL_SIZE_T    oldsize; /* its size */

	mchunkptr newp;             /* chunk to return */
	INTERNAL_SIZE_T    newsize; /* its size */
	Void_t*   newmem;           /* corresponding user mem */

	mchunkptr next;             /* next contiguous chunk after oldp */
	INTERNAL_SIZE_T  nextsize;  /* its size */

	mchunkptr prev;             /* previous contiguous chunk before oldp */
	INTERNAL_SIZE_T  prevsize;  /* its size */

	mchunkptr remainder;        /* holds split off extra space from newp */
	INTERNAL_SIZE_T  remainder_size;   /* its size */

	mchunkptr bck;              /* misc temp for linking */
	mchunkptr fwd;              /* misc temp for linking */

	if ((long)bytes < 0) return 0;

	/* realloc of null is supposed to be same as malloc */
	if (oldmem == 0) return mALLOc(bytes);

	newp    = oldp    = mem2chunk(oldmem);
	newsize = oldsize = chunksize(oldp);


	nb = request2size(bytes);

	check_inuse_chunk(oldp);

	if ((long)(oldsize) < (long)(nb))
	{

		/* Try expanding forward */

		next = chunk_at_offset(oldp, oldsize);
		if (next == top || !inuse(next))
		{
			nextsize = chunksize(next);

			/* Forward into top only if a remainder */
			if (next == top)
			{
				if ((long)(nextsize + newsize) >= (long)(nb + MINSIZE))
				{
					newsize += nextsize;
					top = chunk_at_offset(oldp, nb);
					set_head(top, (newsize - nb) | PREV_INUSE);
					set_head_size(oldp, nb);
					return chunk2mem(oldp);
				}
			}

			/* Forward into next chunk */
			else if (((long)(nextsize + newsize) >= (long)(nb)))
			{
				unlink(next, bck, fwd);
				newsize  += nextsize;
				goto split;
			}
		}
		else
		{
			next = 0;
			nextsize = 0;
		}

		/* Try shifting backwards. */

		if (!prev_inuse(oldp))
		{
			prev = prev_chunk(oldp);
			prevsize = chunksize(prev);

			/* try forward + backward first to save a later consolidation */

			if (next != 0)
			{
				/* into top */
				if (next == top)
				{
					if ((long)(nextsize + prevsize + newsize) >= (long)(nb + MINSIZE))
					{
						unlink(prev, bck, fwd);
						newp = prev;
						newsize += prevsize + nextsize;
						newmem = chunk2mem(newp);
						MALLOC_COPY(newmem, oldmem, oldsize - SIZE_SZ);
						top = chunk_at_offset(newp, nb);
						set_head(top, (newsize - nb) | PREV_INUSE);
						set_head_size(newp, nb);
						return newmem;
					}
				}

				/* into next chunk */
				else if (((long)(nextsize + prevsize + newsize) >= (long)(nb)))
				{
					unlink(next, bck, fwd);
					unlink(prev, bck, fwd);
					newp = prev;
					newsize += nextsize + prevsize;
					newmem = chunk2mem(newp);
					MALLOC_COPY(newmem, oldmem, oldsize - SIZE_SZ);
					goto split;
				}
			}

			/* backward only */
			if (prev != 0 && (long)(prevsize + newsize) >= (long)nb)
			{
				unlink(prev, bck, fwd);
				newp = prev;
				newsize += prevsize;
				newmem = chunk2mem(newp);
				MALLOC_COPY(newmem, oldmem, oldsize - SIZE_SZ);
				goto split;
			}
		}

		/* Must allocate */

		newmem = mALLOc (bytes);

		if (newmem == 0)  /* propagate failure */
			return 0;

		/* Avoid copy if newp is next chunk after oldp. */
		/* (This can only happen when new chunk is sbrk'ed.) */

		if ( (newp = mem2chunk(newmem)) == next_chunk(oldp))
		{
			newsize += chunksize(newp);
			newp = oldp;
			goto split;
		}

		/* Otherwise copy, free, and exit */
		MALLOC_COPY(newmem, oldmem, oldsize - SIZE_SZ);
		fREe(oldmem);
		return newmem;
	}


split:  /* split off extra room in old or expanded chunk */

	if (newsize - nb >= MINSIZE) /* split off remainder */
	{
		remainder = chunk_at_offset(newp, nb);
		remainder_size = newsize - nb;
		set_head_size(newp, nb);
		set_head(remainder, remainder_size | PREV_INUSE);
		set_inuse_bit_at_offset(remainder, remainder_size);
		fREe(chunk2mem(remainder)); /* let free() deal with it */
	}
	else
	{
		set_head_size(newp, newsize);
		set_inuse_bit_at_offset(newp, newsize);
	}

	check_inuse_chunk(newp);
	return chunk2mem(newp);
}



/*

   memalign algorithm:

   memalign requests more than enough space from malloc, finds a spot
   within that chunk that meets the alignment request, and then
   possibly frees the leading and trailing space.

   The alignment argument must be a power of two. This property is not
   checked by memalign, so misuse may result in random runtime errors.

   8-byte alignment is guaranteed by normal malloc calls, so don't
   bother calling memalign with an argument of 8 or less.

   Overreliance on memalign is a sure way to fragment space.

*/
Void_t* mEMALIGn(size_t alignment, size_t bytes)
{
	INTERNAL_SIZE_T    nb;      /* padded  request size */
	char*     m;                /* memory returned by malloc call */
	mchunkptr p;                /* corresponding chunk */
	char*     brk;              /* alignment point within p */
	mchunkptr newp;             /* chunk to return */
	INTERNAL_SIZE_T  newsize;   /* its size */
	INTERNAL_SIZE_T  leadsize;  /* leading space befor alignment point */
	mchunkptr remainder;        /* spare room at end to split off */
	long      remainder_size;   /* its size */

	if ((long)bytes < 0) return 0;

	/* If need less alignment than we give anyway, just relay to malloc */

	if (alignment <= MALLOC_ALIGNMENT) return mALLOc(bytes);

	/* Otherwise, ensure that it is at least a minimum chunk size */

	if (alignment <  MINSIZE) alignment = MINSIZE;

	/* Call malloc with worst case padding to hit alignment. */

	nb = request2size(bytes);
	m  = (char*)(mALLOc(nb + alignment + MINSIZE));

	if (m == 0) return 0; /* propagate failure */

	p = mem2chunk(m);

	if ((((unsigned long)(m)) % alignment) == 0) /* aligned */
	{
	}
	else /* misaligned */
	{
		/*
		   Find an aligned spot inside chunk.
		   Since we need to give back leading space in a chunk of at
		   least MINSIZE, if the first calculation places us at
		   a spot with less than MINSIZE leader, we can move to the
		   next aligned spot -- we've allocated enough total room so that
		   this is always possible.
		   */

		brk = (char*)mem2chunk(((unsigned long)(m + alignment - 1)) & -((signed) alignment));
		if ((long)(brk - (char*)(p)) < MINSIZE) brk = brk + alignment;

		newp = (mchunkptr)brk;
		leadsize = brk - (char*)(p);
		newsize = chunksize(p) - leadsize;

		/* give back leader, use the rest */

		set_head(newp, newsize | PREV_INUSE);
		set_inuse_bit_at_offset(newp, newsize);
		set_head_size(p, leadsize);
		fREe(chunk2mem(p));
		p = newp;

		assert (newsize >= nb && (((unsigned long)(chunk2mem(p))) % alignment) == 0);
	}

	/* Also give back spare room at the end */

	remainder_size = chunksize(p) - nb;

	if (remainder_size >= (long)MINSIZE)
	{
		remainder = chunk_at_offset(p, nb);
		set_head(remainder, remainder_size | PREV_INUSE);
		set_head_size(p, nb);
		fREe(chunk2mem(remainder));
	}

	check_inuse_chunk(p);
	return chunk2mem(p);

}


/*
   valloc just invokes memalign with alignment argument equal
   to the page size of the system (or as near to this as can
   be figured out from all the includes/defines above.)
   */

Void_t* vALLOc(size_t bytes)
{
	return mEMALIGn (malloc_getpagesize, bytes);
}

/*
   pvalloc just invokes valloc for the nearest pagesize
   that will accommodate request
   */
Void_t* pvALLOc(size_t bytes)
{
	size_t pagesize = malloc_getpagesize;
	return mEMALIGn (pagesize, (bytes + pagesize - 1) & ~(pagesize - 1));
}

/*

   calloc calls malloc, then zeroes out the allocated chunk.

*/
Void_t* cALLOc(size_t n, size_t elem_size)
{
	mchunkptr p;
	INTERNAL_SIZE_T csz;

	INTERNAL_SIZE_T sz = n * elem_size;


	/* check if expand_top called, in which case don't need to clear */
#if MORECORE_CLEARS
	mchunkptr oldtop = top;
	INTERNAL_SIZE_T oldtopsize = chunksize(top);
#endif
	Void_t* mem = mALLOc (sz);

	if ((long)n < 0) return 0;

	if (mem == 0)
		return 0;
	else
	{
		p = mem2chunk(mem);

		/* Two optional cases in which clearing not necessary */

		csz = chunksize(p);

#if MORECORE_CLEARS
		if (p == oldtop && csz > oldtopsize)
		{
			/* clear only the bytes from non-freshly-sbrked memory */
			csz = oldtopsize;
		}
#endif

		MALLOC_ZERO(mem, csz - SIZE_SZ);
		return mem;
	}
}

/*

   cfree just calls free. It is needed/defined on some systems
   that pair it with calloc, presumably for odd historical reasons.

*/

#if !defined(INTERNAL_LINUX_C_LIB) || !defined(__ELF__)
void cfree(Void_t *mem)
{
	fREe(mem);
}
#endif


/*

   Malloc_trim gives memory back to the system (via negative
   arguments to sbrk) if there is unused memory at the `high' end of
   the malloc pool. You can call this after freeing large blocks of
   memory to potentially reduce the system-level memory requirements
   of a program. However, it cannot guarantee to reduce memory. Under
   some allocation patterns, some large free blocks of memory will be
   locked between two used chunks, so they cannot be given back to
   the system.

   The `pad' argument to malloc_trim represents the amount of free
   trailing space to leave untrimmed. If this argument is zero,
   only the minimum amount of memory to maintain internal data
   structures will be left (one page or less). Non-zero arguments
   can be supplied to maintain enough trailing space to service
   future expected allocations without having to re-obtain memory
   from the system.

   Malloc_trim returns 1 if it actually released any memory, else 0.

*/

int malloc_trim(size_t pad)
{
	long  top_size;        /* Amount of top-most memory */
	long  extra;           /* Amount to release */
	char* current_brk;     /* address returned by pre-check sbrk call */
	char* new_brk;         /* address returned by negative sbrk call */

	unsigned long pagesz = malloc_getpagesize;

	top_size = chunksize(top);
	extra = ((top_size - pad - MINSIZE + (pagesz-1)) / pagesz - 1) * pagesz;

	if (extra < (long)pagesz)  /* Not enough memory to release */
		return 0;

	else
	{
		/* Test to make sure no one else called sbrk */
		current_brk = (char*)(MORECORE (0));
		if (current_brk != (char*)(top) + top_size)
			return 0;     /* Apparently we don't own memory; must fail */

		else
		{
			new_brk = (char*)(MORECORE (-extra));

			if (new_brk == (char*)(MORECORE_FAILURE)) /* sbrk failed? */
			{
				/* Try to figure out what we have */
				current_brk = (char*)(MORECORE (0));
				top_size = current_brk - (char*)top;
				if (top_size >= (long)MINSIZE) /* if not, we are very very dead! */
				{
					sbrked_mem = current_brk - sbrk_base;
					set_head(top, top_size | PREV_INUSE);
				}
				check_chunk(top);
				return 0;
			}

			else
			{
				/* Success. Adjust top accordingly. */
				set_head(top, (top_size - extra) | PREV_INUSE);
				sbrked_mem -= extra;
				check_chunk(top);
				return 1;
			}
		}
	}
}


/*
malloc_usable_size:

This routine tells you how many bytes you can actually use in an
allocated chunk, which may be more than you requested (although
often not). You can use this many bytes without worrying about
overwriting other allocated objects. Not a particularly great
programming practice, but still sometimes useful.

*/
size_t malloc_usable_size(Void_t* mem)
{
	mchunkptr p;
	if (mem == 0)
		return 0;
	else
	{
		p = mem2chunk(mem);
		if(!chunk_is_mmapped(p))
		{
			if (!inuse(p)) return 0;
			check_inuse_chunk(p);
			return chunksize(p) - SIZE_SZ;
		}
		return chunksize(p) - 2*SIZE_SZ;
	}
}

/*
mallopt:

mallopt is the general SVID/XPG interface to tunable parameters.
The format is to provide a (parameter-number, parameter-value) pair.
mallopt then sets the corresponding parameter to the argument
value if it can (i.e., so long as the value is meaningful),
and returns 1 if successful else 0.

See descriptions of tunable parameters above.

*/
int mALLOPt(int param_number, int value)
{
	switch(param_number)
	{
	case M_TRIM_THRESHOLD:
		trim_threshold = value; return 1;
	case M_TOP_PAD:
		top_pad = value; return 1;
	case M_MMAP_THRESHOLD:
		mmap_threshold = value; return 1;
	case M_MMAP_MAX:
		if (value != 0) return 0; else  n_mmaps_max = value; return 1;

	default:
		return 0;
	}
}
