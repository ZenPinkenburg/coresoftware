/**
 * @file trackbase/TrkrHitSetContainerv2.cc
 * @author D. McGlinchey, H. PEREIRA DA COSTA
 * @date June 2018
 * @brief Implementation for TrkrHitSetContainerv2
 */
#include "TrkrHitSetContainerv2.h"

#include "TrkrDefs.h"
#include "TrkrHitSetv1.h"

#include <TClass.h>

#include <cassert>
#include <cstdlib>

TrkrHitSetContainerv2::
    TrkrHitSetContainerv2(const std::string& hitsetclass)
  : m_hitArray(hitsetclass.c_str())
{
}

void TrkrHitSetContainerv2::Reset()
{
  m_hitmap.clear();
  //! fast clear without calling destructor
  m_hitArray.Clear("C");
}

void TrkrHitSetContainerv2::identify(std::ostream& os) const
{
  syncMapArray();

  os << "TrkrHitSetContainerv2 with class "
     << m_hitArray.GetClass()->GetName()
     << ": Number of hits: " << size() << " index map size = " << m_hitmap.size() << std::endl;
  ConstIterator iter;
  for (const auto& pair : m_hitmap)
  {
    int layer = TrkrDefs::getLayer(pair.first);
    os << "hitsetkey " << pair.first << " layer " << layer << std::endl;
    pair.second->identify();
  }
  return;
}

TrkrHitSetContainerv2::ConstIterator
TrkrHitSetContainerv2::addHitSet(TrkrHitSet* newhit)
{
  std::cout << __PRETTY_FUNCTION__
            << " : deprecated. Use findOrAddHitSet()." << std::endl;
  return addHitSetSpecifyKey(newhit->getHitSetKey(), newhit);
}

TrkrHitSetContainerv2::ConstIterator
TrkrHitSetContainerv2::addHitSetSpecifyKey(const TrkrDefs::hitsetkey key, TrkrHitSet* newhit)
{
  std::cout << __PRETTY_FUNCTION__
            << " : deprecated. Use findOrAddHitSet()." << std::endl;

  exit(1);

  return TrkrHitSetContainer::addHitSetSpecifyKey(key, newhit);
}

void TrkrHitSetContainerv2::removeHitSet(TrkrDefs::hitsetkey key)
{
  std::cout << __PRETTY_FUNCTION__
            << " : deprecated. This function still works but slows down operation." << std::endl;

  syncMapArray();
  auto iter = m_hitmap.find(key);
  if (iter != m_hitmap.end())
  {
    TrkrHitSet* hitset = iter->second;
    //    delete hitset;
    m_hitArray.Remove(hitset);
    m_hitmap.erase(iter);
  }
}

void TrkrHitSetContainerv2::removeHitSet(TrkrHitSet* hitset)
{
  removeHitSet(hitset->getHitSetKey());
}

TrkrHitSetContainerv2::ConstRange
TrkrHitSetContainerv2::getHitSets(const TrkrDefs::TrkrId trackerid) const
{
  syncMapArray();
  const TrkrDefs::hitsetkey keylo = TrkrDefs::getHitSetKeyLo(trackerid);
  const TrkrDefs::hitsetkey keyhi = TrkrDefs::getHitSetKeyHi(trackerid);
  return std::make_pair(m_hitmap.lower_bound(keylo), m_hitmap.upper_bound(keyhi));
}

TrkrHitSetContainerv2::ConstRange
TrkrHitSetContainerv2::getHitSets(const TrkrDefs::TrkrId trackerid, const uint8_t layer) const
{
  syncMapArray();
  TrkrDefs::hitsetkey keylo = TrkrDefs::getHitSetKeyLo(trackerid, layer);
  TrkrDefs::hitsetkey keyhi = TrkrDefs::getHitSetKeyHi(trackerid, layer);
  return std::make_pair(m_hitmap.lower_bound(keylo), m_hitmap.upper_bound(keyhi));
}

TrkrHitSetContainerv2::ConstRange
TrkrHitSetContainerv2::getHitSets() const
{
  syncMapArray();
  return std::make_pair(m_hitmap.cbegin(), m_hitmap.cend());
}

TrkrHitSetContainerv2::Iterator
TrkrHitSetContainerv2::findOrAddHitSet(TrkrDefs::hitsetkey key)
{
  syncMapArray();
  auto it = m_hitmap.lower_bound(key);
  if (it == m_hitmap.end() || (key < it->first))
  {
    TrkrHitSet* hitset = (TrkrHitSet*) m_hitArray.ConstructedAt(m_hitArray.GetSize());
    assert(hitset);

    it = m_hitmap.insert(it, std::make_pair(key, hitset));
    it->second->setHitSetKey(key);
  }
  return it;
}

TrkrHitSet*
TrkrHitSetContainerv2::findHitSet(TrkrDefs::hitsetkey key)
{
  syncMapArray();
  auto it = m_hitmap.find(key);
  if (it != m_hitmap.end())
  {
    return it->second;
  }
  else
  {
    return nullptr;
  }
}

void TrkrHitSetContainerv2::syncMapArray(void) const
{
  if (m_hitmap.size() == (size_t) m_hitArray.GetSize()) return;

  if (m_hitmap.size() > 0)
  {
    std::cout
        << __PRETTY_FUNCTION__ << " Error: m_hitmap and m_hitArray get out of sync, which should not happen unless DST readback. "
        << "m_hitmap.size( ) = " << m_hitmap.size() << " m_hitArray.GetSize() = " << m_hitArray.GetSize() << std::endl;
  }

  for (int i = 0; i < m_hitArray.GetSize(); ++i)
  {
    TrkrHitSet* hitset = static_cast<TrkrHitSet*>(m_hitArray[i]);

    assert(hitset);

    m_hitmap[hitset->getHitSetKey()] = hitset;
  }
}
