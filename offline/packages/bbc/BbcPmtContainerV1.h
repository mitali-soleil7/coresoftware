// Tell emacs that this is a C++ source
//  -*- C++ -*-.
#ifndef BBC_BBCPMTCONTAINERV1_H
#define BBC_BBCPMTCONTAINERV1_H

#include "BbcPmtContainer.h"

class TClonesArray;

///
class BbcPmtContainerV1 : public BbcPmtContainer
{
public:
  /// ctor
  BbcPmtContainerV1();

  /// dtor
  virtual ~BbcPmtContainerV1();

  /// Clear Event
  void Reset() override;

  /** identify Function from PHObject
      @param os Output Stream 
   */
  void identify(std::ostream& os = std::cout) const override;

  /// isValid returns non zero if object contains vailid data
  int isValid() const override;

 
  /** set T0 for Bbc
      @param ival Number of Bbc Pmt's
   */
  void set_npmt(const Short_t ival) override {npmt=ival;return;}

  /// get Number of Bbc Pmt's
  Short_t get_npmt() const override {return npmt;}

  /** get id of Pmt iPmt in TClonesArray
      @param iPmt no of Pmt in TClonesArray
   */
  Short_t get_pmt(const int iPmt) const override;

  /** get Adc of Pmt iPmt in TClonesArray
      @param iPmt no of Pmt in TClonesArray
   */
  float get_adc(const int iPmt) const override;

  /** get Tdc0 of Pmt iPmt in TClonesArray
      @param iPmt no of Pmt in TClonesArray
   */
  float get_tdc0(const int iPmt) const override;

  /** get Tdc1 of Pmt iPmt in TClonesArray
      @param iPmt no of Pmt in TClonesArray
   */
  float get_tdc1(const int iPmt) const override;

  /** Add Bbc Raw hit object to TCLonesArray
      @param pmt Pmt id
      @param adc Adc value
      @param tdc0 Tdc0 value
      @param tdc1 Tdc1 value
      @param ipmt no of pmt
  */
   void AddBbcPmt(const Short_t ipmt, const float adc, const Float_t tdc0, const Float_t tdc1) override;

private:
  TClonesArray *GetBbcPmtHits() const {return BbcPmtHits;}

  Short_t npmt = 0;
  TClonesArray *BbcPmtHits = nullptr;


  ClassDefOverride(BbcPmtContainerV1,1)
};

#endif


