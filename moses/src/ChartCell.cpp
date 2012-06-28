// $Id$
// vim:tabstop=2
/***********************************************************************
 Moses - factored phrase-based language decoder
 Copyright (C) 2010 Hieu Hoang

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**********************************************************************/
#include <algorithm>
#include "ChartCell.h"
#include "Classifier.h"
#include "ChartTranslationOptionCollection.h"
#include "ChartCellCollection.h"
#include "RuleCubeQueue.h"
#include "RuleCube.h"
#include "WordsRange.h"
#include "Util.h"
#include "StaticData.h"
#include "ChartTranslationOption.h"
#include "ChartTranslationOptionList.h"
#include "ChartManager.h"

using namespace std;

namespace Moses
{
extern bool g_debug;

ChartCell::ChartCell(size_t startPos, size_t endPos, ChartManager &manager)
  :m_coverage(startPos, endPos)
  ,m_sourceWordLabel(NULL)
  ,m_targetLabelSet(m_coverage)
  ,m_manager(manager)
{
  const StaticData &staticData = StaticData::Instance();
  m_nBestIsEnabled = staticData.IsNBestEnabled();
  if (startPos == endPos) {
    const Word &sourceWord = manager.GetSource().GetWord(startPos);
    m_sourceWordLabel = new ChartCellLabel(m_coverage, sourceWord);
  }
}

ChartCell::~ChartCell()
{
  delete m_sourceWordLabel;
}

/** Add the given hypothesis to the cell */
bool ChartCell::AddHypothesis(ChartHypothesis *hypo)
{
  const Word &targetLHS = hypo->GetTargetLHS();
  return m_hypoColl[targetLHS].AddHypothesis(hypo, m_manager);
}

/** Pruning */
void ChartCell::PruneToSize()
{
  MapType::iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    ChartHypothesisCollection &coll = iter->second;
    coll.PruneToSize(m_manager);
  }
}

/** Decoding at span level: fill chart cell with hypotheses
 *  (implementation of cube pruning)
 * \param transOptList list of applicable rules to create hypotheses for the cell
 * \param allChartCells entire chart - needed to look up underlying hypotheses
 */
void ChartCell::ProcessSentence(const ChartTranslationOptionList &transOptList
                                , const ChartCellCollection &allChartCells)
{
  const StaticData &staticData = StaticData::Instance();

  // priority queue for applicable rules with selected hypotheses
  RuleCubeQueue queue(m_manager);

  // add all trans opt into queue. using only 1st child node.
  for (size_t i = 0; i < transOptList.GetSize(); ++i) {
    const ChartTranslationOption &transOpt = transOptList.Get(i);
    RuleCube *ruleCube = new RuleCube(transOpt, allChartCells, m_manager);
    queue.Add(ruleCube);
  }

  // pluck things out of queue and add to hypo collection
  const size_t popLimit = staticData.GetCubePruningPopLimit();
  for (size_t numPops = 0; numPops < popLimit && !queue.IsEmpty(); ++numPops)
  {
    ChartHypothesis *hypo = queue.Pop();
    AddHypothesis(hypo);
  }
}

//! (damt hiero) : process sentence and consider context (word to left)
void ChartCell::ProcessSentenceWithContext(const ChartTranslationOptionList &transOptList, const ChartCellCollection &allChartCells)
{

  std::cout << "Processing Sentence with context..." << std::endl;

  const StaticData &staticData = StaticData::Instance();

  // priority queue for applicable rules with selected hypotheses
  RuleCubeQueue queue(m_manager);

  //Get source-sentence Context in form of a vector of words
  std::vector<Word> sourceContext;

  //Determine factored representation
  vector<FactorType> srcFactors;
  vector<FactorType> tgtFactors;

  srcFactors.push_back(0);
  tgtFactors.push_back(0);

  for(int i=0; i < m_manager.GetSource().GetSize(); i++)
    {
      sourceContext.push_back(m_manager.GetSource().GetWord(i));
    }

  // add all trans opt into queue. using only 1st child node.
  for (size_t i = 0; i < transOptList.GetSize(); ++i) {
    const ChartTranslationOption &transOpt = transOptList.Get(i);
    float score = 0;

    std::cout <<  "Size of target phrase collection : " << transOpt.GetTargetPhraseCollection().GetSize() << std::endl;

    TargetPhraseCollection::const_iterator itr_targets;
    //BEWARE : to access the source side and score of a rule, we have to access each target phrase
    for(
        itr_targets = transOpt.GetTargetPhraseCollection().begin();
        itr_targets != transOpt.GetTargetPhraseCollection().end();
        itr_targets++)
    {

        //make example
      //make source vector
      std::vector<std::string> sourceWords;
      std::vector<std::string> targetWords;

      //can create everything locally
      //TargetPhrase target = **itr_targets;
      //std::cout << "Target Phrase : " << target << std::endl;

      //const Phrase *source = target.GetSourcePhrase();

      if((**itr_targets).GetSourcePhrase() != NULL)
      {
            std::cout << "Source Phrase : " << (*(**itr_targets).GetSourcePhrase()) << std::endl;

            for(int i=0; i<(**itr_targets).GetSourcePhrase()->GetSize();i++)
            {
                sourceWords.push_back((**itr_targets).GetSourcePhrase()->GetWord(i).GetString(srcFactors,0));
            }
            for(int i=0; i<(**itr_targets).GetSize();i++)
            {
                //make target vector
                targetWords.push_back((**itr_targets).GetWord(i).GetString(srcFactors,0));
            }
            ClassExample exampleToSearch = ClassExample(sourceContext,sourceWords,targetWords, &(*itr_targets)->GetAlignmentInfo());

            //call classifier for this exampl
            std::cout << "Getting Prediction : " << std::endl;
            float score = Classifier::Instance().GetPrediction(exampleToSearch);
            std::cout << "Prediction here" << std::endl;

            //FB : TODO : set score

            //FB : new feature will be evaluated automatically at hypothesis creation
            //FB : no need to reset score of translation option, will not be used again
            //FB : pruning using score of translation options performed before but pruning
            //limit can be increased in moses.ini
        }
        else
        {
            std::cout << "Source Phrase is NULL : " << (**itr_targets) << std::endl;
        }
    }
    RuleCube *ruleCube = new RuleCube(transOpt, allChartCells, m_manager);
    queue.Add(ruleCube);
  }

  // pluck things out of queue and add to hypo collection
  const size_t popLimit = staticData.GetCubePruningPopLimit();
  for (size_t numPops = 0; numPops < popLimit && !queue.IsEmpty(); ++numPops)
  {
    ChartHypothesis *hypo = queue.Pop();
    AddHypothesis(hypo);
  }
}

void ChartCell::SortHypotheses()
{
  // sort each mini cells & fill up target lhs list
  CHECK(m_targetLabelSet.Empty());
  MapType::iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    ChartHypothesisCollection &coll = iter->second;
    m_targetLabelSet.AddConstituent(iter->first, coll);
    coll.SortHypotheses();
  }
}

/** Return the highest scoring hypothesis in the cell */
const ChartHypothesis *ChartCell::GetBestHypothesis() const
{
  const ChartHypothesis *ret = NULL;
  float bestScore = -std::numeric_limits<float>::infinity();

  MapType::const_iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    const HypoList &sortedList = iter->second.GetSortedHypotheses();
    CHECK(sortedList.size() > 0);

    const ChartHypothesis *hypo = sortedList[0];
    if (hypo->GetTotalScore() > bestScore) {
      bestScore = hypo->GetTotalScore();
      ret = hypo;
    };
  }

  return ret;
}

void ChartCell::CleanupArcList()
{
  // only necessary if n-best calculations are enabled
  if (!m_nBestIsEnabled) return;

  MapType::iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    ChartHypothesisCollection &coll = iter->second;
    coll.CleanupArcList();
  }
}

void ChartCell::OutputSizes(std::ostream &out) const
{
  MapType::const_iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    const Word &targetLHS = iter->first;
    const ChartHypothesisCollection &coll = iter->second;

    out << targetLHS << "=" << coll.GetSize() << " ";
  }
}

size_t ChartCell::GetSize() const
{
  size_t ret = 0;
  MapType::const_iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    const ChartHypothesisCollection &coll = iter->second;

    ret += coll.GetSize();
  }

  return ret;
}

void ChartCell::GetSearchGraph(long translationId, std::ostream &outputSearchGraphStream, const std::map<unsigned, bool> &reachable) const
{
  MapType::const_iterator iterOutside;
  for (iterOutside = m_hypoColl.begin(); iterOutside != m_hypoColl.end(); ++iterOutside) {
    const ChartHypothesisCollection &coll = iterOutside->second;
    coll.GetSearchGraph(translationId, outputSearchGraphStream, reachable);
  }
}

std::ostream& operator<<(std::ostream &out, const ChartCell &cell)
{
  ChartCell::MapType::const_iterator iterOutside;
  for (iterOutside = cell.m_hypoColl.begin(); iterOutside != cell.m_hypoColl.end(); ++iterOutside) {
    const Word &targetLHS = iterOutside->first;
    cerr << targetLHS << ":" << endl;

    const ChartHypothesisCollection &coll = iterOutside->second;
    cerr << coll;
  }

  /*
  ChartCell::HCType::const_iterator iter;
  for (iter = cell.m_hypos.begin(); iter != cell.m_hypos.end(); ++iter)
  {
  	const ChartHypothesis &hypo = **iter;
  	out << hypo << endl;
  }
   */

  return out;
}

} // namespace
