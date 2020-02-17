#include "tabfunctions.hpp"

#include <QInputDialog>
#include <QMessageBox>

#include "guiutils.hpp"
#include "logger.hpp"
#include "sbml.hpp"
#include "ui_tabfunctions.h"

TabFunctions::TabFunctions(sbml::SbmlDocWrapper &doc, QWidget *parent)
    : QWidget(parent), ui{std::make_unique<Ui::TabFunctions>()}, sbmlDoc(doc) {
  ui->setupUi(this);
  connect(ui->listFunctions, &QListWidget::currentRowChanged, this,
          &TabFunctions::listFunctions_currentRowChanged);

  connect(ui->btnAddFunction, &QPushButton::clicked, this,
          &TabFunctions::btnAddFunction_clicked);

  connect(ui->btnRemoveFunction, &QPushButton::clicked, this,
          &TabFunctions::btnRemoveFunction_clicked);

  connect(ui->listFunctionParams, &QListWidget::currentRowChanged, this,
          &TabFunctions::listFunctionParams_currentRowChanged);

  connect(ui->btnAddFunctionParam, &QPushButton::clicked, this,
          &TabFunctions::btnAddFunctionParam_clicked);

  connect(ui->btnRemoveFunctionParam, &QPushButton::clicked, this,
          &TabFunctions::btnRemoveFunctionParam_clicked);

  connect(ui->txtFunctionDef, &QPlainTextMathEdit::mathChanged, this,
          &TabFunctions::txtFunctionDef_mathChanged);

  connect(ui->btnSaveFunctionChanges, &QPushButton::clicked, this,
          &TabFunctions::btnSaveFunctionChanges_clicked);
}

TabFunctions::~TabFunctions() = default;

void TabFunctions::loadModelData(const QString &selection) {
  auto *list = ui->listFunctions;
  list->clear();
  ui->btnRemoveFunctionParam->setEnabled(false);
  for (const auto &funcID : sbmlDoc.functions) {
    list->addItem(sbmlDoc.getFunctionDefinition(funcID).name.c_str());
  }
  selectMatchingOrFirstItem(list, selection);
  bool enable = list->count() > 0;
  ui->txtFunctionName->setEnabled(enable);
  ui->listFunctionParams->setEnabled(enable);
  ui->txtFunctionDef->setEnabled(enable);
}

void TabFunctions::listFunctions_currentRowChanged(int row) {
  ui->txtFunctionName->clear();
  ui->listFunctionParams->clear();
  ui->txtFunctionDef->clear();
  ui->lblFunctionDefStatus->clear();
  ui->btnSaveFunctionChanges->setEnabled(false);
  if ((row < 0) || (row > sbmlDoc.functions.size() - 1)) {
    ui->btnAddFunctionParam->setEnabled(false);
    ui->btnRemoveFunctionParam->setEnabled(false);
    ui->btnRemoveFunction->setEnabled(false);
    return;
  }
  auto funcId = sbmlDoc.functions.at(row);
  SPDLOG_DEBUG("Function {} selected", funcId.toStdString());
  auto func = sbmlDoc.getFunctionDefinition(funcId);
  ui->txtFunctionName->setText(func.name.c_str());
  ui->txtFunctionDef->setVariables(func.arguments);
  for (const auto &argument : func.arguments) {
    ui->listFunctionParams->addItem(argument.c_str());
  }
  if (!func.arguments.empty()) {
    ui->listFunctionParams->setCurrentRow(0);
  } else {
    ui->btnRemoveFunctionParam->setEnabled(false);
  }
  ui->txtFunctionDef->setPlainText(func.expression.c_str());
  ui->btnAddFunctionParam->setEnabled(true);
  ui->btnRemoveFunctionParam->setEnabled(!func.arguments.empty());
  ui->btnRemoveFunction->setEnabled(true);
}

void TabFunctions::btnAddFunction_clicked() {
  bool ok;
  auto functionName = QInputDialog::getText(
      this, "Add function", "New function name:", QLineEdit::Normal, {}, &ok);
  if (ok) {
    sbmlDoc.addFunction(functionName);
    loadModelData(functionName);
  }
}

void TabFunctions::btnRemoveFunction_clicked() {
  int row = ui->listFunctions->currentRow();
  if ((row < 0) || (row > sbmlDoc.functions.size() - 1)) {
    return;
  }
  auto msgbox =
      newYesNoMessageBox("Remove function?",
                         QString("Remove function '%1' from the model?")
                             .arg(ui->listFunctions->currentItem()->text()),
                         this);
  connect(msgbox, &QMessageBox::finished, this,
          [&s = sbmlDoc, row, this](int result) {
            if (result == QMessageBox::Yes) {
              auto funcId = s.functions.at(row);
              SPDLOG_INFO("Removing function {}", funcId.toStdString());
              s.removeFunction(funcId);
              this->loadModelData();
            }
          });
  msgbox->open();
}

void TabFunctions::listFunctionParams_currentRowChanged(int row) {
  bool valid = (row >= 0) && (row < ui->listFunctionParams->count());
  ui->btnRemoveFunctionParam->setEnabled(valid);
}

void TabFunctions::btnAddFunctionParam_clicked() {
  bool ok;
  auto param =
      QInputDialog::getText(this, "Add function parameter",
                            "New parameter name:", QLineEdit::Normal, {}, &ok);
  if (ok && !param.isEmpty()) {
    SPDLOG_INFO("Adding parameter {}", param.toStdString());
    SPDLOG_INFO("- todo: check valid alphanumeric variable name");
    ui->listFunctionParams->addItem(param);
    ui->txtFunctionDef->addVariable(param.toStdString());
    ui->listFunctionParams->setCurrentRow(ui->listFunctionParams->count() - 1);
  }
}

void TabFunctions::btnRemoveFunctionParam_clicked() {
  int row = ui->listFunctionParams->currentRow();
  if ((row < 0) || (row > ui->listFunctionParams->count() - 1)) {
    return;
  }
  auto *item = ui->listFunctionParams->currentItem();
  const auto param = item->text();
  auto msgbox = newYesNoMessageBox(
      "Remove function parameter?",
      QString("Remove function parameter '%1' from the model?").arg(param),
      this);
  connect(msgbox, &QMessageBox::finished, this,
          [item, f = ui->txtFunctionDef](int result) {
            if (result == QMessageBox::Yes) {
              auto p = item->text().toStdString();
              SPDLOG_INFO("Removing parameter {}", p);
              delete item;
              f->removeVariable(p);
            }
          });
  msgbox->open();
}

void TabFunctions::txtFunctionDef_mathChanged(const QString &math, bool valid,
                                              const QString &errorMessage) {
  ui->btnSaveFunctionChanges->setEnabled(valid);
  if (valid) {
    SPDLOG_INFO("new math: {}", math.toStdString());
    ui->lblFunctionDefStatus->setText("");
  } else {
    SPDLOG_INFO("math err: {}", errorMessage.toStdString());
    ui->lblFunctionDefStatus->setText(errorMessage);
  }
}

void TabFunctions::btnSaveFunctionChanges_clicked() {
  if (!ui->txtFunctionDef->mathIsValid()) {
    return;
  }
  int row = ui->listFunctions->currentRow();
  auto func = sbmlDoc.getFunctionDefinition(sbmlDoc.functions.at(row));
  SPDLOG_INFO("Updating function {}", func.id);
  func.name = ui->txtFunctionName->text().toStdString();
  SPDLOG_INFO("  - name: {}", func.name);
  int nParams = ui->listFunctionParams->count();
  func.arguments.clear();
  func.arguments.reserve(static_cast<std::size_t>(nParams));
  for (int i = 0; i < nParams; ++i) {
    func.arguments.push_back(
        ui->listFunctionParams->item(i)->text().toStdString());
    SPDLOG_INFO("  - arg: {}", func.arguments.back());
  }
  func.expression = ui->txtFunctionDef->getMath().toStdString();
  SPDLOG_INFO("  - expression: {}", func.expression);
  sbmlDoc.setFunctionDefinition(func);
  loadModelData(func.name.c_str());
}
