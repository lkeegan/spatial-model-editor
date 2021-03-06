#include "catch_wrapper.hpp"
#include "model.hpp"
#include "model_reactions.hpp"
#include <QFile>
#include <QImage>
#include <memory>
#include <sbml/SBMLTypes.h>
#include <sbml/extension/SBMLDocumentPlugin.h>
#include <sbml/packages/spatial/common/SpatialExtensionTypes.h>
#include <sbml/packages/spatial/extension/SpatialExtension.h>

using namespace sme;

SCENARIO("SBML reactions",
         "[core/model/reactions][core/model][core][model][reactions]") {
  GIVEN("ModelReactions") {
    model::Model model;
    QFile f(":/models/very-simple-model.xml");
    f.open(QIODevice::ReadOnly);
    std::string xml{f.readAll().toStdString()};
    model.importSBMLString(xml);
    auto &r{model.getReactions()};
    r.setHasUnsavedChanges(false);
    REQUIRE(r.getHasUnsavedChanges() == false);
    REQUIRE(r.getIds("c1").empty());
    REQUIRE(r.getIds("c2").empty());
    REQUIRE(r.getIds("c3").size() == 1);
    REQUIRE(r.getIds("c3")[0] == "A_B_conversion");
    REQUIRE(r.getParameterIds("A_B_conversion").size() == 1);
    REQUIRE(r.getIds("c1_c2_membrane").size() == 2);
    REQUIRE(r.getIds("c1_c2_membrane")[0] == "A_uptake");
    REQUIRE(r.getParameterIds("A_uptake").size() == 1);
    REQUIRE(r.getIds("c1_c2_membrane")[1] == "B_excretion");
    REQUIRE(r.getParameterIds("B_excretion").size() == 1);
    REQUIRE(r.getIds("c2_c3_membrane").size() == 2);
    REQUIRE(r.getIds("c2_c3_membrane")[0] == "A_transport");
    REQUIRE(r.getParameterIds("A_transport").size() == 2);
    REQUIRE(r.getIds("c2_c3_membrane")[1] == "B_transport");
    REQUIRE(r.getParameterIds("B_transport").size() == 1);

    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "A_c1") == dbl_approx(-1.0));
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "B_c1") == dbl_approx(0.0));
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "A_c2") == dbl_approx(1.0));
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "B_c2") == dbl_approx(0.0));
    REQUIRE(r.getScheme("A_uptake") == "A_out -> A_cell");
    // move reaction to cell: A_out no longer valid reaction species so removed
    r.setLocation("A_uptake", "c2");
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "A_c1") == dbl_approx(0.0));
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "B_c1") == dbl_approx(0.0));
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "A_c2") == dbl_approx(1.0));
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "B_c2") == dbl_approx(0.0));
    REQUIRE(r.getScheme("A_uptake") == " -> A_cell");
    // move reaction to outside: A_cell no longer valid & removed
    r.setLocation("A_uptake", "c1");
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "A_c1") == dbl_approx(0.0));
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "B_c1") == dbl_approx(0.0));
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "A_c2") == dbl_approx(0.0));
    REQUIRE(r.getSpeciesStoichiometry("A_uptake", "B_c2") == dbl_approx(0.0));
    REQUIRE(r.getScheme("A_uptake") == "");
    // move reaction back to starting location
    r.setLocation("A_uptake", "c1_c2_membrane");
    REQUIRE(r.getScheme("A_uptake") == "");

    // remove parameter from non-existent reaction: no-op
    r.setHasUnsavedChanges(false);
    REQUIRE(r.getHasUnsavedChanges() == false);
    r.removeParameter("i_dont_exist", "i_dont_exist");
    REQUIRE(r.getHasUnsavedChanges() == false);

    // remove non-existent reaction parameter: no-op
    r.setHasUnsavedChanges(false);
    REQUIRE(r.getHasUnsavedChanges() == false);
    REQUIRE(r.getParameterIds("A_B_conversion").size() == 1);
    r.removeParameter("A_B_conversion", "i_dont_exist");
    REQUIRE(r.getParameterIds("A_B_conversion").size() == 1);
    REQUIRE(r.getHasUnsavedChanges() == false);

    // remove reaction parameter
    r.removeParameter("A_B_conversion", "k1");
    REQUIRE(r.getParameterIds("A_B_conversion").size() == 0);
    REQUIRE(r.getHasUnsavedChanges() == true);

    // remove reaction
    r.setHasUnsavedChanges(false);
    REQUIRE(r.getHasUnsavedChanges() == false);
    r.remove("A_B_conversion");
    REQUIRE(r.getHasUnsavedChanges() == true);
    REQUIRE(r.getIds("c1").empty());
    REQUIRE(r.getIds("c2").empty());
    REQUIRE(r.getIds("c3").empty());
    REQUIRE(r.getIds("c1_c2_membrane").size() == 2);
    REQUIRE(r.getIds("c1_c2_membrane")[0] == "A_uptake");
    REQUIRE(r.getParameterIds("A_uptake").size() == 1);
    REQUIRE(r.getIds("c1_c2_membrane")[1] == "B_excretion");
    REQUIRE(r.getParameterIds("B_excretion").size() == 1);
    REQUIRE(r.getIds("c2_c3_membrane").size() == 2);
    REQUIRE(r.getIds("c2_c3_membrane")[0] == "A_transport");
    REQUIRE(r.getParameterIds("A_transport").size() == 2);
    REQUIRE(r.getIds("c2_c3_membrane")[1] == "B_transport");
    REQUIRE(r.getParameterIds("B_transport").size() == 1);

    // remove reaction
    r.remove("A_uptake");
    REQUIRE(r.getIds("c1").empty());
    REQUIRE(r.getIds("c2").empty());
    REQUIRE(r.getIds("c3").empty());
    REQUIRE(r.getIds("c1_c2_membrane").size() == 1);
    REQUIRE(r.getIds("c1_c2_membrane")[0] == "B_excretion");
    REQUIRE(r.getParameterIds("B_excretion").size() == 1);
    REQUIRE(r.getIds("c2_c3_membrane").size() == 2);
    REQUIRE(r.getIds("c2_c3_membrane")[0] == "A_transport");
    REQUIRE(r.getParameterIds("A_transport").size() == 2);
    REQUIRE(r.getIds("c2_c3_membrane")[1] == "B_transport");
    REQUIRE(r.getParameterIds("B_transport").size() == 1);

    // remove reaction
    r.remove("B_excretion");
    REQUIRE(r.getIds("c1").empty());
    REQUIRE(r.getIds("c2").empty());
    REQUIRE(r.getIds("c3").empty());
    REQUIRE(r.getIds("c1_c2_membrane").empty());
    REQUIRE(r.getIds("c2_c3_membrane").size() == 2);
    REQUIRE(r.getIds("c2_c3_membrane")[0] == "A_transport");
    REQUIRE(r.getParameterIds("A_transport").size() == 2);
    REQUIRE(r.getIds("c2_c3_membrane")[1] == "B_transport");
    REQUIRE(r.getParameterIds("B_transport").size() == 1);

    // remove reaction
    r.remove("A_transport");
    REQUIRE(r.getIds("c1").empty());
    REQUIRE(r.getIds("c2").empty());
    REQUIRE(r.getIds("c3").empty());
    REQUIRE(r.getIds("c1_c2_membrane").empty());
    REQUIRE(r.getIds("c2_c3_membrane").size() == 1);
    REQUIRE(r.getIds("c2_c3_membrane")[0] == "B_transport");
    REQUIRE(r.getParameterIds("B_transport").size() == 1);

    // remove non-existent reaction: no-op
    r.setHasUnsavedChanges(false);
    REQUIRE(r.getHasUnsavedChanges() == false);
    r.remove("i_dont_exist");
    REQUIRE(r.getIds("c1").empty());
    REQUIRE(r.getIds("c2").empty());
    REQUIRE(r.getIds("c3").empty());
    REQUIRE(r.getIds("c1_c2_membrane").empty());
    REQUIRE(r.getIds("c2_c3_membrane").size() == 1);
    REQUIRE(r.getIds("c2_c3_membrane")[0] == "B_transport");
    REQUIRE(r.getParameterIds("B_transport").size() == 1);
    REQUIRE(r.getHasUnsavedChanges() == false);

    // remove reaction
    r.remove("B_transport");
    REQUIRE(r.getHasUnsavedChanges() == true);
    REQUIRE(r.getIds("c1").empty());
    REQUIRE(r.getIds("c2").empty());
    REQUIRE(r.getIds("c3").empty());
    REQUIRE(r.getIds("c1_c2_membrane").empty());
    REQUIRE(r.getIds("c2_c3_membrane").empty());
  }
  GIVEN("Model with spatial reactions whose location is not set") {
    QFile f(":test/models/fish_300x300.xml");
    f.open(QIODevice::ReadOnly);
    std::string xml{f.readAll().toStdString()};
    std::unique_ptr<libsbml::SBMLDocument> doc(
        libsbml::readSBMLFromString(xml.c_str()));
    {
      const auto *reac0{doc->getModel()->getReaction("re0")};
      REQUIRE(reac0->isSetCompartment() == false);
      const auto *reac1{doc->getModel()->getReaction("re1")};
      REQUIRE(reac1->isSetCompartment() == false);
      const auto *reac2{doc->getModel()->getReaction("re2")};
      REQUIRE(reac2->isSetCompartment() == false);
    }
    model::Model m;
    m.importSBMLString(xml);
    const auto &reactions{m.getReactions()};
    // reaction location inferred from species involved
    REQUIRE(reactions.getIds("fish").size() == 3);
    REQUIRE(reactions.getIds("fish")[0] == "re0");
    REQUIRE(reactions.getIds("fish")[1] == "re1");
    REQUIRE(reactions.getIds("fish")[2] == "re2");
    {
      auto xml2{m.getXml().toStdString()};
      std::unique_ptr<libsbml::SBMLDocument> doc2(
          libsbml::readSBMLFromString(xml2.c_str()));
      const auto *reac0{doc2->getModel()->getReaction("re0")};
      REQUIRE(reac0->isSetCompartment() == true);
      REQUIRE(reac0->getCompartment() == "fish");
      const auto *reac1{doc2->getModel()->getReaction("re1")};
      REQUIRE(reac1->isSetCompartment() == true);
      REQUIRE(reac1->getCompartment() == "fish");
      const auto *reac2{doc2->getModel()->getReaction("re2")};
      REQUIRE(reac2->isSetCompartment() == true);
      REQUIRE(reac2->getCompartment() == "fish");
    }
  }
  GIVEN("Model with spatial reaction whose name & location is not set") {
    QFile f(":test/models/example2D.xml");
    f.open(QIODevice::ReadOnly);
    std::string xml{f.readAll().toStdString()};
    std::unique_ptr<libsbml::SBMLDocument> doc(
        libsbml::readSBMLFromString(xml.c_str()));
    const auto *reac{doc->getModel()->getReaction(0)};
    REQUIRE(reac->isSetCompartment() == false);
    REQUIRE(reac->isSetName() == false);
    model::Model m;
    m.importSBMLString(xml);
    const auto &reactions{m.getReactions()};
    // reaction location inferred from species involved
    REQUIRE(reactions.getIds("Nucleus_Cytosol_membrane").size() == 1);
    REQUIRE(reactions.getIds("Nucleus_Cytosol_membrane")[0] == "re1");
    // reaction name uses Id if not originally set
    REQUIRE(reactions.getName("re1") == "re1");
    auto xml2{m.getXml().toStdString()};
    std::unique_ptr<libsbml::SBMLDocument> doc2(
        libsbml::readSBMLFromString(xml2.c_str()));
    const auto *reac2{doc2->getModel()->getReaction(0)};
    REQUIRE(reac2->isSetCompartment() == true);
    REQUIRE(reac2->getCompartment() == "Nucleus_Cytosol_membrane");
    REQUIRE(reac2->isSetName() == true);
    REQUIRE(reac2->getName() == reac2->getId());
  }
}
