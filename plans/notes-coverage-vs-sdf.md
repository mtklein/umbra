Coverage-interval vs SDF-interval: an open architectural question
==================================================================

Status: open; recording the discussion, not resolving it.

What got us here
----------------

Plan 02 landed a quadtree dispatcher that intervalizes the **coverage**
(α) output of a draw pipeline.  iv2d's scheme is adjacent but not
identical: it intervalizes a signed **region** function (f), with f < 0
meaning inside, and derives coverage from f's sign at the leaves.

Our scheme interprets the downstream signal (α) directly.  iv2d's
interprets an upstream signal (f) and derives α.  In principle ours
is strictly more general — anything that produces α fits, whether or
not an f exists.  In practice, "more general" is doing more work than
that phrase suggests.

What we gain by intervalizing α directly
----------------------------------------

Things we can *express* that a strict umbra_sdf scheme couldn't:

1. **Coverages that are α, not derivations of α.**
   Text glyph bitmaps (the texture IS α), alpha stencils from decoded
   images, grayscale masks.  No signed function to interval over.

2. **Compositional on α, not on f.**
   Fractional intersection = `α_a · α_b`, Porter-Duff blends, soft-shadow
   decays like `α = exp(-d²/σ²)`.  None correspond directly to
   min/max/sub on signed fields.

3. **Winding-rule rendering** (Slug's bezier coverage).
   Signed-area accumulation emits α directly, cheaper than first
   constructing an SDF for the curves.

4. **Shader-driven supersampling.**
   `umbra_shader_supersample` takes sub-pixel taps and averages; there's
   no f underneath that.

5. **Arbitrary per-pixel accuracy at partial tiles.**
   Our dispatcher bottoms out in a shader that computes α bit-exactly
   per pixel.  iv2d bottoms out in a closed-form α estimate (-lo/(hi-lo))
   at sub-pixel leaves.  For coverages that *are* SDFs this is roughly
   a wash; for coverages that aren't, we give bit-exact results iv2d
   couldn't produce.

What we give up
---------------

iv2d has several properties built in that we have to earn piecemeal:

1. **Free Lipschitz bounds.**
   True SDFs have `|∇f| ≤ 1` by construction.  One sample at the tile
   center plus the tile diameter gives a bound, no extra machinery.
   Arbitrary α has no analogous guarantee.

2. **Interval-fraction α as the AA mechanism.**
   iv2d's `-lo/(hi - lo)` is cheap, principled, and bottoms out
   sub-pixel recursion with a good estimate.  Our AA comes from the
   partial-coverage shader — which means every coverage author is on
   the hook for their own AA quality at the leaf.

3. **Natural CSG combinators.**
   `union = min(f_a, f_b)` on SDFs is a one-op IR sugar; our equivalent
   (`max(α_a, α_b)`?  `α_a + α_b - α_a·α_b`?) depends on which blend
   semantics you want and doesn't reduce as cleanly.

4. **Sub-pixel recursion that terminates cheaply.**
   iv2d can recurse below 1 pixel for quality and stop at the
   interval-fraction formula.  We'd need the partial-coverage shader
   to handle sub-pixel-scale dispatch, which it isn't currently
   designed for.

The honest asymmetry
--------------------

"Our scheme expresses more" ≠ "our scheme usefully intervalizes more."
Most of the non-SDF cases I listed need per-coverage machinery to
produce usefully tight bounds — and that machinery ends up looking
roughly like iv2d's SDF math applied to a different input:

  - **Text bitmap α**: needs an α-pyramid precomputation per atlas.
  - **Winding-rule coverage**: needs a per-tile curve-BVH query.
  - **Bitmap SDF sampler**: either mimic iv2d's Lipschitz bound
    (requires trusting |∇sdf|) or build a lo/hi pyramid.
  - **Procedural α**: works if the math happens to be interval-
    friendly.  Minority of real workloads.

So the set of coverages we can *usefully* intervalize is roughly
the union of (SDF-like things, done with iv2d's math anyway) and
(things with precomputed spatial metadata, done with per-coverage
custom bound methods).  The framework is more general; the tight-
bound set overlaps heavily with iv2d's natural territory.

Two futures for the current architecture
----------------------------------------

**Charitable.**  The dispatch substrate is neutral.  We invest
seriously in plan 04 (`umbra_sdf` / `umbra_region`) as a first-class
concept with its own `bound()` method, default Lipschitz bound, CSG
combinators, and interval-fraction α baked in.  Non-SDF coverages
plug into the same substrate via per-coverage `bound` methods.  We
get iv2d's strengths *on top of* a system that also handles the
things iv2d can't.

**Uncharitable.**  We shipped a general mechanism that's serviceable
but not ideal for anything.  SDF-specific optimizations never get
first-class treatment because they're "just one kind of coverage";
they land as bolt-ons that underperform a dedicated system.  Non-SDF
use cases that justified the generality never materialize in
meaningful volume.  We end up with a system that's OK at everything
and great at nothing.

The concrete ask: elevate umbra_sdf
-----------------------------------

The charitable future needs plan 04 to land differently than
originally sketched.  Not "umbra_coverage_region is another
umbra_coverage" — that reads as afterthought and invites the
uncharitable future.  Instead:

    struct umbra_sdf {
        umbra_val32 (*f)    (struct umbra_sdf*, struct umbra_builder*,
                             struct umbra_uniforms_layout*,
                             umbra_val32 x, umbra_val32 y);
        interval    (*bound)(struct umbra_sdf const*, interval x, interval y,
                             float const *uniforms);   // default: Lipschitz on f
        void        (*fill) (struct umbra_sdf const*, void *uniforms);
    };

    // Subtypes, landing in one plan:
    struct umbra_sdf_circle     umbra_sdf_circle(...);       // analytic
    struct umbra_sdf_rect       umbra_sdf_rect(...);         // analytic
    struct umbra_sdf_union      umbra_sdf_union(...);        // combinator
    struct umbra_sdf_intersect  umbra_sdf_intersect(...);    // combinator
    struct umbra_sdf_difference umbra_sdf_difference(...);   // combinator
    struct umbra_sdf_bitmap     umbra_sdf_bitmap(...);       // sampled

    // Adapter that makes an umbra_sdf present as an umbra_coverage,
    // including the interval-fraction α at pixel level (plan 05
    // collapses into this).
    struct umbra_coverage_sdf umbra_coverage_from_sdf(struct umbra_sdf*);

Plan 04 then reads as "elevate SDFs to a first-class type with all
the iv2d-style machinery baked in" rather than "add one more
coverage implementation."  Plan 05 stops being a separate plan and
becomes part of `umbra_coverage_from_sdf`'s implementation.  The
`bound` callback generalizes in plan 06 (or wherever) to cover
non-SDF coverages with their own spatial metadata.

Open items to revisit
---------------------

- **Real workload mix.**  What do umbra's intended users actually
  draw?  If the honest answer is "SDFs almost exclusively with some
  text mixed in," the uncharitable future is a real risk and we
  should consider whether the current substrate is over-engineered
  for what it will actually serve.  If the answer is "mixed 2D
  rendering — analytic shapes, text, image compositing, stylized
  effects" the charitable future maps naturally to the problem.

- **Bitmap SDF intervals specifically.**  Lipschitz bound vs lo/hi
  pyramid — which do we ship?  Probably Lipschitz-by-default with an
  opt-in pyramid for non-Lipschitz atlases.  Worth concrete
  measurement before committing.

- **Coverage types that never usefully intervalize.**  If Slug's
  winding coverage or umbra_coverage_bitmap turn out to never produce
  tight tile-level bounds even with structural help, we should be
  honest that they get flat dispatch forever — and the "our scheme
  handles them" argument loses most of its weight.

- **Retrofit cost of going SDF-native.**  If we ever decide our
  substrate was the wrong call, what does it take to rework around
  `umbra_sdf` as the primary concept?  Probably: keep the dispatcher,
  rename coverage→sdf at the authoring API, keep umbra_coverage as a
  thin adapter going the other direction.  Not catastrophic, but
  worth thinking through before commitment grows.

What this note is and isn't
---------------------------

This isn't a plan; there's no action to take directly from it.
It's recording the architectural tension so that future work on
plans 04 / 05 / 06 can reference back to it instead of re-deriving
the arguments.  Worth re-reading when:

  - drafting plan 04's Expectations block,
  - deciding whether plan 05 is still a plan or just implementation
    inside 04,
  - seeing a real workload profile that points one way or the other.
