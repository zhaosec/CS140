#include "page.h"


static unsigned
suppl_pte_hash_func (const struct hash_elem *e, void *aux UNUSED)
{
  struct suppl_pte *spte = hash_entry (e, struct suppl_pte, elem_hash);
  return (int) spte->upage;
}

static bool
suppl_pte_hash_less (const struct hash_elem *a,
                     const struct hash_elem *b,
                     void *aux UNUSED)
{
  struct suppl_pte *spte_a = hash_entry (a, struct suppl_pte, elem_hash);
  struct suppl_pte *spte_b = hash_entry (b, struct suppl_pte, elem_hash);
  return spte_a->upage < spte_b->upage;
}

void
suppl_pt_init (struct hash *suppl_pt)
{
  hash_init (suppl_pt, suppl_pte_hash_func, suppl_pte_hash_less, NULL);
}