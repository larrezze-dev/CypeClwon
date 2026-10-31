#include "LimitStatesDialog.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "Icons.h"
#include "SituationsDialog.h"

namespace cc::app {

using namespace cc::core;

namespace {

QString qs(const std::string& s) { return QString::fromStdString(s); }

QLabel* boldLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(QStringLiteral("<b>%1</b>").arg(text.toHtmlEscaped()), parent);
    return label;
}

QFrame* separator(QWidget* parent) {
    auto* line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

}  // namespace

LimitStatesDialog::LimitStatesDialog(const StandardsSelection& standards, const LimitStatesSettings& initial,
                                     QWidget* parent)
    : CypeDialog(tr("Estados límite"), parent), m_base(initial) {
    setFixedSize(358, 438);
    QVBoxLayout* body = contentLayout();
    body->setSpacing(3);

    const QString concrete = qs(shortStandardName(standards.concrete));
    body->addWidget(boldLabel(tr("Hormigón: %1").arg(concrete), this));
    body->addWidget(boldLabel(tr("Hormigón en cimentaciones: %1").arg(concrete), this));
    body->addWidget(boldLabel(tr("E.L.U. de rotura. Aluminio: %1").arg(qs(shortStandardName(standards.aluminium))), this));
    body->addWidget(boldLabel(tr("Tensiones sobre el terreno"), this));

    ReportContext context;
    context.concrete = shortStandardName(standards.concrete);
    context.aluminium = shortStandardName(standards.aluminium);
    context.coldFormed = shortStandardName(standards.coldFormedSteel);
    context.timber = shortStandardName(standards.timber);
    using ReportBuilder = CombinationReport (*)(bool, const ReportContext&);
    const auto snowRow = [this, body, context](const QString& label, const std::string& current, int comboWidth,
                                               ReportBuilder builder) {
        auto* row = new QHBoxLayout;
        row->addSpacing(10);
        row->addWidget(new QLabel(label, this));
        auto* combo = new QComboBox(this);
        for (const std::string& item : LimitStatesSettings::snowChoices()) combo->addItem(qs(item));
        combo->setCurrentText(qs(current));
        combo->setFixedWidth(comboWidth);
        row->addWidget(combo);
        row->addStretch(1);
        auto* pencil = new QToolButton(this);
        pencil->setIcon(themedIcon(QStringLiteral("edit_pencil")));
        pencil->setAutoRaise(true);
        if (builder) {
            pencil->setToolTip(tr("Ver las situaciones y combinaciones de acciones"));
            connect(pencil, &QToolButton::clicked, this, [this, combo, builder, context] {
                const bool high = combo->currentText().toStdString() == LimitStatesSettings::snowChoices().front();
                SituationsDialog(builder(high, context), this).exec();
            });
        } else {
            pencil->setEnabled(false);  // report not captured in the original yet (TK-017)
            pencil->setToolTip(tr("Informe pendiente de capturar en el original (ticket TK-017)."));
        }
        row->addWidget(pencil);
        body->addLayout(row);
        return combo;
    };
    m_ground = snowRow(tr("Nieve"), initial.groundSnow, 205, &buildGroundReport);
    body->addWidget(separator(this));

    body->addWidget(boldLabel(tr("E.L.U. de rotura. Acero conformado: %1").arg(qs(shortStandardName(standards.coldFormedSteel))), this));
    body->addWidget(boldLabel(tr("E.L.U. de rotura. Madera: %1").arg(qs(shortStandardName(standards.timber))), this));
    // Cold-formed steel and timber are listed together and share one snow combo in the original.
    m_coldFormed = snowRow(tr("Cota de nieve"), initial.coldFormedSnow, 205, &buildColdFormedTimberReport);
    body->addWidget(separator(this));

    body->addWidget(boldLabel(tr("E.L.U. de rotura. Acero laminado: %1").arg(qs(shortStandardName(standards.hotRolledSteel))), this));
    auto* codes = new QHBoxLayout;
    codes->addSpacing(10);
    m_cte = new QRadioButton(tr("CTE"), this);
    auto* ec = new QRadioButton(tr("EC"), this);
    (initial.hotRolledCode == CombinationCode::CTE ? m_cte : ec)->setChecked(true);
    codes->addWidget(m_cte);
    codes->addWidget(ec);
    codes->addStretch(1);
    body->addLayout(codes);
    m_hotRolled = snowRow(tr("Cota de nieve"), initial.hotRolledSnow, 205, nullptr);
    body->addWidget(separator(this));

    body->addWidget(boldLabel(tr("Desplazamientos"), this));
    auto* displacement = new QHBoxLayout;
    displacement->addSpacing(10);
    displacement->addWidget(new QLabel(tr("Acciones características"), this));
    displacement->addStretch(1);
    auto* pencil = new QToolButton(this);
    pencil->setIcon(themedIcon(QStringLiteral("edit_pencil")));
    pencil->setAutoRaise(true);
    pencil->setEnabled(false);
    pencil->setToolTip(tr("Diálogo pendiente de capturar en el original (ticket TK-017)."));
    displacement->addWidget(pencil);
    body->addLayout(displacement);

    auto* configure = new QPushButton(tr("Configurar combinaciones para cada estado límite"), this);
    body->addWidget(configure);
    connect(configure, &QPushButton::clicked, this, [this] { LimitStatesVerifyDialog(value(), this).exec(); });
    body->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

LimitStatesSettings LimitStatesDialog::value() const {
    LimitStatesSettings out = m_base;
    out.groundSnow = m_ground->currentText().toStdString();
    out.coldFormedSnow = m_coldFormed->currentText().toStdString();
    out.timberSnow = out.coldFormedSnow;  // one shared combo
    out.hotRolledSnow = m_hotRolled->currentText().toStdString();
    out.hotRolledCode = m_cte->isChecked() ? CombinationCode::CTE : CombinationCode::EC;
    return out;
}

LimitStatesVerifyDialog::LimitStatesVerifyDialog(const LimitStatesSettings& settings, QWidget* parent)
    : CypeDialog(tr("Estados límite a verificar"), parent) {
    setFixedSize(398, 356);
    auto* grid = new QGridLayout;
    grid->setColumnMinimumWidth(0, 158);
    grid->setHorizontalSpacing(2);
    grid->setColumnStretch(1, 1);
    const QString captions[] = {tr("Hormigón"),         tr("Cimentación"),
                                tr("Acero conformado"), tr("Acero laminado"),
                                tr("Madera"),           tr("Aluminio"),
                                tr("Tensiones sobre el terreno"), tr("Desplazamientos")};
    int row = 0;
    for (const LimitStatesSettings::Row& r : settings.rows()) {
        auto* button = new QPushButton(captions[row], this);
        button->setEnabled(false);  // editor dialogs not captured (TK-017)
        button->setToolTip(tr("Diálogo pendiente de capturar en el original (ticket TK-017)."));
        grid->addWidget(button, row, 0);
        QString value = qs(r.value);
        value.replace(qs(LimitStatesSettings::defaultSnow()), tr("Altitud inferior o igual a 1000 m"));
        value.replace(QStringLiteral("Acciones características"), tr("Acciones características"));
        grid->addWidget(new QLabel(value, this), row, 1);
        ++row;
    }
    contentLayout()->addLayout(grid);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

}  // namespace cc::app
