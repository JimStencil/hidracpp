#include "hidragui.h"
#include "ui_hidragui.h"

HidraGui::HidraGui(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::HidraGui)
{
    ui->setupUi(this);

    //CODIGO PARA BETA VERSION
    codeEditor = new HidraCodeEditor();
    connect(codeEditor, SIGNAL(textChanged()), this, SLOT(sourceCodeChanged()));
    ui->layoutSourceCodeHolder->addWidget(codeEditor);

    highlighter = new HidraHighlighter(codeEditor->document());

    //FIM DO BETA CODE
    currentFile = "";
    modifiedFile = false;
    sourceAndMemoryInSync = false;
    model = NULL;
    machine = NULL;

    ui->layoutRegisters->setAlignment(Qt::AlignTop);
    ui->scrollAreaRegisters->setFrameShape(QFrame::NoFrame);
    ui->tableViewMemoryInstructions->setEditTriggers(false);

    // limpa a interface, e seta a maquina selecionada como o neander
    ui->comboBoxMachine->setCurrentIndex(0);
}

HidraGui::~HidraGui()
{
    delete ui;
}

void HidraGui::initializeFlagWidgets()
{
    for (int i=0; i < machine->getNumberOfFlags(); i++)
    {
        FlagWidget *newFlag = new FlagWidget(this, machine->getFlagName(i), machine->getFlagValue(i));
        ui->layoutFlags->addWidget(newFlag);
        flagWidgets.append(newFlag);
    }
}

void HidraGui::initializeRegisterWidgets()
{
    for (int i=0; i < machine->getNumberOfRegisters(); i++)
    {
        RegisterWidget *newRegister = new RegisterWidget(this, machine->getRegisterName(i));
        ui->layoutRegisters->addWidget(newRegister, i/2, i%2); // Two per line, alternates left and right columns with i%2
        registerWidgets.append(newRegister);
    }
}

void HidraGui::initializeMachineInterface()
{
    clearMachineInterface();
    initializeFlagWidgets();
    initializeRegisterWidgets();
}

void HidraGui::clearFlagWidgets()
{
    while(ui->layoutFlags->count() > 0)
        delete ui->layoutFlags->takeAt(0)->widget();

    flagWidgets.clear();
}

void HidraGui::clearRegisterWidgets()
{
    while(ui->layoutRegisters->count() > 0)
        delete ui->layoutRegisters->takeAt(0)->widget();

    registerWidgets.clear();
}

void HidraGui::clearMachineInterface()
{
    clearRegisterWidgets();
    clearFlagWidgets();
}

void HidraGui::save()
{
    QFile file(currentFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::information(this, "Incapaz de abrir arquivo",
                                 file.errorString());
        fileSaved = false;
        return;
    }

    fileSaved = true;
    QTextStream out(&file);
    out << codeEditor->toPlainText();
    file.close();

    // Save memory
    machine->save(currentFile.section(".", 0, -2).append(".mem"));

    modifiedFile = false;
}

void HidraGui::saveAs()
{
    QString ext;
    switch (ui->comboBoxMachine->currentIndex()) {
    case 0:
        ext = "Fonte do Neander (*.ndr)";
        break;
    case 1:
        ext = "Fonte do Ahmes (*.ahd)";
        break;
    case 2:
        ext = "Fonte do Ramses (*.rms)";
        break;
    default:
        break;
    }
    currentFile = QFileDialog::getSaveFileName(this,
                                               "Salvar código-fonte", "",
                                               ext);

    if (currentFile.isEmpty()) {
        fileSaved = false;
        return;
    }
    else {
        save(); // Resets fileModified to false if successful
    }
}

void HidraGui::updateMemoryMap()
{
    delete model;
    model = new QStandardItemModel(NeanderMachine::MEM_SIZE, 3, this);  //temp gamby, will change

    model->setHeaderData(0, Qt::Horizontal, " ");
    model->setHeaderData(1, Qt::Horizontal, "End");
    model->setHeaderData(2, Qt::Horizontal, "Valor");
    QVector<Byte *> auxMem = machine->getMemory();
    int i = 0;
    QModelIndex index;
    foreach (Byte* tmp, auxMem) {
        index = model->index(i,1);
        model->setData(index, i);

        index = model->index(i,2);
        model->setData(index, tmp->getValue());
        i++;

    }
    index = model->index(machine->getPCValue(), 0);
    model->setData(index, QString::fromUtf8("\u2192")); // Unicode arrow

    ui->tableViewMemoryInstructions->setModel(model);
    ui->tableViewMemoryInstructions->resizeColumnsToContents();
    ui->tableViewMemoryInstructions->resizeRowsToContents();
    ui->tableViewMemoryInstructions->verticalHeader()->hide();
}

void HidraGui::updateFlagWidgets()
{
    for (int i=0; i<flagWidgets.count(); i++)
        flagWidgets.at(i)->setValue(machine->getFlagValue(i));
}

void HidraGui::updateRegisterWidgets()
{
    for (int i=0; i<registerWidgets.count(); i++)
        registerWidgets.at(i)->setValue(machine->getRegisterValue(i));
}

void HidraGui::updateMachineInterface()
{
    updateMemoryMap();
    updateFlagWidgets();
    updateRegisterWidgets();
}

void HidraGui::cleanErrorsField()
{
    ui->textEditError->clear();
}

void HidraGui::addError(QString errorString)
{
    ui->textEditError->setPlainText(ui->textEditError->toPlainText() + errorString + "\n");
}

void HidraGui::on_pushButtonStep_clicked(){
    ui->actionPasso->trigger();
}

void HidraGui::on_pushButtonRun_clicked(){
    ui->actionRodar->trigger();
}

void HidraGui::on_actionPasso_triggered()
{
    machine->step();
    updateMachineInterface();
}

void HidraGui::on_actionRodar_triggered()
{
    machine->run();
    updateMachineInterface();
}

void HidraGui::on_actionMontar_triggered()
{
    cleanErrorsField();
    machine->assemble(codeEditor->toPlainText());
    updateMachineInterface();

    if (machine->buildSuccessful)
        sourceAndMemoryInSync = true;
}

void HidraGui::on_actionSaveAs_triggered()
{
    saveAs();
}

void HidraGui::on_comboBoxMachine_currentIndexChanged(int index)
{
    delete machine;
    switch (index) {
    case 0:
        machine = new NeanderMachine();
        break;
    case 1:
        machine = new AhmesMachine();
        break;
    case 2:
        machine = new RamsesMachine();
        break;
    case 3:
        //machine = new CesarMachine();
        machine = new RamsesMachine();  //evita  o crash
        break;
    default:
        break;
    }
    if(index != 3) {
        connect(machine, SIGNAL(buildErrorDetected(QString)), this, SLOT(addError(QString)));
        highlighter->setTargetMachine(machine);
    }

    initializeMachineInterface();
}

void HidraGui::on_action_Save_triggered()
{
    if(currentFile == "") {
        saveAs();
    } else {
        save();
    }
}

void HidraGui::on_actionClose_triggered()
{
    this->close();
}

void HidraGui::closeEvent(QCloseEvent *event)
{
    bool cancelled = false;

    // Se o arquivo foi modificado, oferece para salvar alterações
    if (modifiedFile)
    {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Hidra",
                                      "Deseja salvar as alterações feitas?",
                                      QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);

        if (reply == QMessageBox::Cancel)
            cancelled = true;

        if (reply == QMessageBox::Yes)
        {
            ui->action_Save->trigger();

            if (modifiedFile) // Se o arquivo não foi salvo no diálogo (ainda está modificado), cancela
                cancelled = true;
        }
    }

    // Aceita ou rejeita o evento que fecha a janela
    if (!cancelled)
        event->accept();
    else
        event->ignore();
}

void HidraGui::on_actionManual_triggered()
{

}

void HidraGui::on_actionRelatar_um_problema_triggered()
{

}

void HidraGui::on_actionOpen_triggered()
{
    QString ext;
    switch (ui->comboBoxMachine->currentIndex()) {
    case 0:
        ext = "Fonte do Neander (*.ndr)";
        break;
    case 1:
        ext = "Fonte do Ahmes (*.ahd)";
        break;
    case 2:
        ext = "Fonte do Ramses (*.rms)";
        break;
    default:
        break;
    }
    currentFile = QFileDialog::getOpenFileName(this,
                                               "Abrir código-fonte", "",
                                               ext);

    if (currentFile.isEmpty())
        return;
    else {
        QFile file(currentFile);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::information(this, tr("Incapaz de abrir arquivo"),
                                     file.errorString());
            return;
        }
        QTextStream in(&file);
        codeEditor->setPlainText(in.readAll());
        modifiedFile = false;
        file.close();
    }
}

void HidraGui::sourceCodeChanged()
{
    sourceAndMemoryInSync = false;
    modifiedFile = true;
}

void HidraGui::on_actionCarregar_triggered()
{

}

void HidraGui::on_actionSaveMem_triggered()
{

}

void HidraGui::on_actionZerarMemoria_triggered()
{
    machine->clearMemory();
    updateMachineInterface();
}

void HidraGui::on_actionZerar_registradores_triggered()
{
    machine->clearRegisters();
    updateMachineInterface();
}

void HidraGui::on_pushButtonMontar_clicked()
{
    ui->actionMontar->trigger();
}
