#include "simulate.hpp"

#include "logger.hpp"
#include "reactions.hpp"

namespace simulate {

ReacEval::ReacEval(sbml::SbmlDocWrapper *doc_ptr,
                   const std::vector<std::string> &speciesIDs,
                   const std::vector<std::string> &reactionIDs)
    : doc(doc_ptr) {
  spdlog::info("ReacEval::ReacEval :: species vector: {}", speciesIDs);

  // construct reaction expressions and stoich matrix
  reactions::Reaction reactions(doc_ptr, speciesIDs, reactionIDs);
  M = reactions.M;
  nReactions = reactions.reacExpressions.size();

  // init vector of species
  species_values = std::vector<double>(speciesIDs.size(), 0.0);
  result = species_values;
  nSpecies = species_values.size();

  // compile reaction expressions
  reac_eval.clear();
  reac_eval.reserve(reactions.reacExpressions.size());
  for (std::size_t reac_index = 0;
       reac_index < reactions.reacExpressions.size(); ++reac_index) {
    reac_eval.emplace_back(
        numerics::ExprEval(reactions.reacExpressions[reac_index], speciesIDs,
                           species_values, reactions.constants[reac_index]));
  }
}

void ReacEval::evaluate() {
  std::fill(result.begin(), result.end(), 0.0);
  for (std::size_t j = 0; j < nReactions; ++j) {
    // evaluate reaction term
    double r_j = reac_eval[j]();
    // qDebug("ReacEval::ReacEval :: R[%lu] = %f", j, r_j);
    for (std::size_t s = 0; s < nSpecies; ++s) {
      // add contribution for each species to result
      result[s] += M[j][s] * r_j;
    }
  }
}

SimCompartment::SimCompartment(sbml::SbmlDocWrapper *docWrapper,
                               const geometry::Compartment *compartment)
    : doc(docWrapper), comp(compartment) {
  QString compID = compartment->compartmentID.c_str();
  spdlog::debug("SimCompartment::SimCompartment :: compartment: {}",
                compID.toStdString());
  std::vector<std::string> speciesID;
  std::size_t nRateRules = 0;
  for (const auto &s : doc->species.at(compID)) {
    if (!doc->isSpeciesConstant(s.toStdString())) {
      speciesID.push_back(s.toStdString());
      field.push_back(&doc->mapSpeciesIdToField.at(s));
      spdlog::debug("SimCompartment::SimCompartment :: - adding field: {}",
                    s.toStdString());
      if (doc->model->getRateRule(s.toStdString()) != nullptr) {
        ++nRateRules;
      }
    }
  }
  const auto iter = doc->reactions.find(compID);
  if ((nRateRules == 0) &&
      (iter == doc->reactions.cend() || doc->reactions.at(compID).empty())) {
    // If there are no reactions or RateRules in this compartment: we are done
    spdlog::debug(
        "SimCompartment::SimCompartment ::   - no Reactions or RateRules to "
        "compile.");
    return;
  }
  std::vector<std::string> reactionID;
  if (iter != doc->reactions.cend()) {
    for (const auto &reac : iter->second) {
      reactionID.push_back(reac.toStdString());
    }
  }
  reacEval = ReacEval(doc, speciesID, reactionID);
}

void SimCompartment::evaluate_reactions() {
  // qDebug("SimCompartment::evaluate_reactions : compartment: %s",
  //       field->geometry->compartmentID.c_str());
  if (reacEval.nReactions == 0) {
    // qDebug("SimCompartment::evaluate_reactions :   - no reactions to
    // evaluate");
    return;
  }
  // qDebug("SimCompartment::evaluate_reactions :   - n_pixels=%lu",
  // field->n_pixels);
  // qDebug("SimCompartment::evaluate_reactions :   - n_species=%lu",
  //       reacEval.nSpecies);
  // qDebug("SimCompartment::evaluate_reactions :   - n_reacs=%lu",
  //       reacEval.nReactions);
  for (std::size_t i = 0; i < comp->ix.size(); ++i) {
    // populate species concentrations
    for (std::size_t s = 0; s < field.size(); ++s) {
      reacEval.species_values[s] = field[s]->conc[i];
    }
    // evaluate reaction terms
    reacEval.evaluate();
    const std::vector<double> &result = reacEval.getResult();
    for (std::size_t s = 0; s < field.size(); ++s) {
      // add results to dcdt
      field[s]->dcdt[i] += result[s];
    }
  }
}

SimMembrane::SimMembrane(sbml::SbmlDocWrapper *doc_ptr,
                         geometry::Membrane *membrane_ptr)
    : doc(doc_ptr), membrane(membrane_ptr) {
  QString compA = membrane->compA->compartmentID.c_str();
  QString compB = membrane->compB->compartmentID.c_str();
  spdlog::debug("SimMembrane::compile_reactions :: membrane: {}",
                membrane->membraneID);
  spdlog::debug("SimMembrane::compile_reactions ::   - compA: {}",
                compA.toStdString());
  spdlog::debug("SimMembrane::compile_reactions ::   - compB: {}",
                compB.toStdString());

  if (doc->reactions.find(membrane->membraneID.c_str()) ==
          doc->reactions.cend() ||
      doc->reactions.at(membrane->membraneID.c_str()).empty()) {
    // If there are no reactions in this membrane: we are done
    // qDebug("SimMembrane::compile_reactions ::   - no reactions to compile.");
    return;
  }

  // make vector of species & fields from compartments A and B
  std::vector<std::string> speciesID;
  for (const auto &spec : doc->species.at(compA)) {
    if (!doc->isSpeciesConstant(spec.toStdString())) {
      speciesID.push_back(spec.toStdString());
      fieldA.push_back(&doc->mapSpeciesIdToField.at(spec));
    }
  }
  for (const auto &spec : doc->species.at(compB)) {
    if (!doc->isSpeciesConstant(spec.toStdString())) {
      speciesID.push_back(spec.toStdString());
      fieldB.push_back(&doc->mapSpeciesIdToField.at(spec));
    }
  }

  // make vector of reactions from membrane
  std::vector<std::string> reactionID;
  for (const auto &reac : doc->reactions.at(membrane->membraneID.c_str())) {
    reactionID.push_back(reac.toStdString());
  }

  reacEval = ReacEval(doc, speciesID, reactionID);
}

void SimMembrane::evaluate_reactions() {
  // qDebug("SimMembrane::evaluate_reactions : membrane: %s",
  //         membrane->membraneID.c_str());
  if (reacEval.nReactions == 0) {
    // qDebug("SimMembrane::evaluate_reactions :   - no reactions to
    // evaluate");
    return;
  }
  // qDebug("SimMembrane::evaluate_reactions :   - n_pixel pairs=%lu",
  //    membrane->indexPair.size());
  // qDebug("SimMembrane::evaluate_reactions :   - n_species=%lu",
  //       reacEval.nSpecies);
  // qDebug("SimMembrane::evaluate_reactions :   - n_reacs=%lu",
  //       reacEval.nReactions);
  assert(reacEval.species_values.size() == fieldA.size() + fieldB.size());
  for (const auto &p : membrane->indexPair) {
    std::size_t ixA = p.first;
    std::size_t ixB = p.second;
    // populate species concentrations: first A, then B
    std::size_t reacIndex = 0;
    for (const auto *fA : fieldA) {
      reacEval.species_values[reacIndex] = fA->conc[ixA];
      ++reacIndex;
    }
    for (const auto *fB : fieldB) {
      reacEval.species_values[reacIndex] = fB->conc[ixB];
      ++reacIndex;
    }
    // evaluate reaction terms
    reacEval.evaluate();
    const std::vector<double> &result = reacEval.getResult();
    // add results to dc/dt: first A, then B
    reacIndex = 0;
    for (auto *fA : fieldA) {
      fA->dcdt[ixA] += result[reacIndex];
      ++reacIndex;
    }
    for (auto *fB : fieldB) {
      fB->dcdt[ixB] += result[reacIndex];
      ++reacIndex;
    }
  }
}

void Simulate::addCompartment(geometry::Compartment *compartment) {
  spdlog::debug("Simulate::addCompartment :: adding compartment {}",
                compartment->compartmentID);
  simComp.emplace_back(doc, compartment);
  for (auto *f : simComp.back().field) {
    field.push_back(f);
    speciesID.push_back(f->speciesID);
    spdlog::debug("Simulate::addCompartment ::   - adding species {}",
                  f->speciesID);
  }
}

void Simulate::addMembrane(geometry::Membrane *membrane) {
  spdlog::debug("Simulate::addMembrane :: adding membrane {}",
                membrane->membraneID);
  simMembrane.emplace_back(doc, membrane);
}

void Simulate::integrateForwardsEuler(double dt) {
  // apply Diffusion operator in all compartments: dc/dt = D ...
  for (auto *f : field) {
    if (f->isSpatial) {
      f->applyDiffusionOperator();
    } else {
      std::fill(f->dcdt.begin(), f->dcdt.end(), 0.0);
    }
  }
  // evaluate reaction terms in all compartments: dc/dt += ...
  for (auto &sim : simComp) {
    sim.evaluate_reactions();
  }
  // evaluate reaction terms in all membranes: dc/dt += ...
  for (auto &sim : simMembrane) {
    sim.evaluate_reactions();
  }
  // for non-spatial species: spatially average dc/dt:
  // roughly equivalent to infinite rate of diffusion
  for (auto *f : field) {
    if (!f->isSpatial) {
      double av_dcdt = std::accumulate(f->dcdt.cbegin(), f->dcdt.cend(), 0.0) /
                       static_cast<double>(f->dcdt.size());
      std::fill(f->dcdt.begin(), f->dcdt.end(), av_dcdt);
    }
  }
  // forwards Euler timestep: c += dt * (dc/dt) in all compartments
  for (auto *f : field) {
    for (std::size_t i = 0; i < f->conc.size(); ++i) {
      f->conc[i] += dt * f->dcdt[i];
    }
  }
}

QImage Simulate::getConcentrationImage() {
  QImage img(field[0]->geometry->getCompartmentImage().size(),
             QImage::Format_ARGB32);
  img.fill(qRgba(0, 0, 0, 0));
  // iterate over compartments
  for (const auto &comp : simComp) {
    // normalise species concentration:
    // max value of each species = max colour intensity
    // with lower bound, so constant zero is still zero
    std::vector<double> max_conc;
    for (const auto *f : comp.field) {
      double m = *std::max_element(f->conc.cbegin(), f->conc.cend());
      max_conc.push_back(m < 1e-15 ? 1.0 : m);
    }
    // equal contribution from each field
    double alpha = 1.0 / static_cast<double>(comp.field.size());
    for (std::size_t i = 0; i < comp.field[0]->geometry->ix.size(); ++i) {
      const QPoint &p = comp.field[0]->geometry->ix[i];
      int r = 0;
      int g = 0;
      int b = 0;
      for (std::size_t i_f = 0; i_f < comp.field.size(); ++i_f) {
        const auto *f = comp.field[i_f];
        double c = alpha * f->conc[i] / max_conc[i_f];
        r += static_cast<int>(f->red * c);
        g += static_cast<int>(f->green * c);
        b += static_cast<int>(f->blue * c);
      }
      img.setPixel(p, qRgb(r, g, b));
    }
  }
  return img;
}

}  // namespace simulate
