#ifndef FASTER_RNNLM_NCE_H_
#define FASTER_RNNLM_NCE_H_

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <vector>

#include "faster-rnnlm/settings.h"
#include "faster-rnnlm/util.h"

class CudaStorage;
class MaxEnt;
class HSTree;
class INoiseGenerator;
class Vocabulary;


static const int kMaxNoiseSamples = 1024;


// Noise samples are generated by noise generators
// and passed to NCE::PropagateForwardAndBackward
struct NoiseSample {
  WordIndex noise_words[kMaxNoiseSamples];
  Real noise_ln_probabilities[kMaxNoiseSamples];
  Real target_ln_probability;
  int size;
};


class NCE {
 public:
    class Updater {
     public:
      Updater(NCE* nce): nce_(nce), embedding_grad_(nce->layer_size_) {}

      void PropagateForwardAndBackward(
          const Ref<const RowVector> hidden, WordIndex target_word,
          const uint64_t* maxent_indices, size_t maxent_size,
          const NoiseSample& sample, Real lrate, Real l2reg,
          Real maxent_lrate, Real maxent_l2reg, Real gradient_clipping,
          Ref<RowVector> hidden_grad, MaxEnt* maxent);

     private:
      NCE* nce_;
      RowVector embedding_grad_;
    };

    NCE(
        bool use_cuda, bool use_cuda_memory_efficient,
        Real zln, int layer_size, const Vocabulary&, uint64_t maxent_hash_size);

    ~NCE();

    int DetectEffectiveMaxentOrder(
        WordIndex target_word, const MaxEnt* maxent,
        const uint64_t* maxent_indices, size_t maxent_size) const;

    void UploadNetWeightsToCuda(const MaxEnt* maxent);

    void CalculateLog10ProbabilityBatch(
        const Ref<const RowMatrix> hidden_layers, const MaxEnt* maxent,
        const uint64_t* maxent_indices_all, const int* maxent_indices_count_all,
        const WordIndex* sentence, int sentence_length,
        const bool do_not_normalize,
        std::vector<Real>* logprob_per_pos);

    // Calculate unnormalized probability of a word
    Real CalculateWordLnScore(
        const Ref<const RowVector> hidden, const MaxEnt* maxent,
        const uint64_t* maxent_indices, int maxent_indices_count,
        WordIndex target_word) const;

    void Dump(FILE* fo) const;

    void Load(FILE* fo);

 private:
    const Real zln_;
    const int layer_size_, vocab_size_;
    const uint64_t maxent_hash_size_;

    RowMatrix sm_embedding_;

#ifdef NOCUDA
    static const bool use_cuda_ = false;
#else
    CudaStorage* cust_;
    const bool use_cuda_;
#endif

    friend class Updater;
};


class INoiseGenerator {
 public:
    virtual uint64_t PrepareNoiseSample(
        uint64_t random_state, int n_samples, const WordIndex* sen,
        int sen_pos, NoiseSample* sample) const = 0;

    virtual ~INoiseGenerator() {}
};


class UnigramNoiseGenerator : public INoiseGenerator {
 public:
    UnigramNoiseGenerator(const Vocabulary& vocab, Real noise_power, Real noise_min_cell);

    virtual uint64_t PrepareNoiseSample(
        uint64_t random_state, int n_samples, const WordIndex* sen,
        int sen_pos, NoiseSample* sample) const;


 private:
    static const uint32_t kUnigramTableSize = 1e8;

    const Real noise_power_, noise_min_cells_;

    std::vector<WordIndex> unigram_table_;
    std::vector<double> ln_probabilities_;
};


class HSMaxEntNoiseGenerator : public INoiseGenerator {
 public:
    HSMaxEntNoiseGenerator(
        const HSTree* tree, const MaxEnt* maxent_layer,
        uint64_t maxent_hash_size, int vocab_size, int maxent_order);

    virtual uint64_t PrepareNoiseSample(
        uint64_t random_state, int n_samples, const WordIndex* sen,
        int sen_pos, NoiseSample* sample) const;


 private:
    const HSTree* tree_;
    const MaxEnt* maxent_layer_;
    const uint64_t maxent_hash_size_;
    const int vocab_size_, maxent_order_;
};

#endif  // FASTER_RNNLM_NCE_H_

