#pragma once
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QWidget>
#include <functional>

namespace cc::app::messages {

/// Message windows of the original: «Información» (blue "i"), «Aviso» (yellow triangle), «Error» (red) with the single button "Aceptar", and
/// «Pregunta» with "Sí" (default) / "No". Everything that the diaphragm and bar-end tools say goes through here so that tests can replace
/// the handler (nothing blocks offscreen). The handler returns true for "Sí" / "Aceptar".
enum class Kind { Information, Warning, Error, Question, QuestionDefaultNo };
using Handler = std::function<bool(QWidget* parent, Kind kind, const QString& text)>;

inline Handler& handler() {
    static Handler instance;
    return instance;
}

inline bool show(QWidget* parent, Kind kind, const QString& text) {
    if (handler()) return handler()(parent, kind, text);
    QMessageBox box(parent);
    box.setText(text);
    switch (kind) {
        case Kind::Information:
            box.setIcon(QMessageBox::Information);
            box.setWindowTitle(QObject::tr("Información"));
            box.addButton(QObject::tr("Aceptar"), QMessageBox::AcceptRole);
            break;
        case Kind::Warning:
            box.setIcon(QMessageBox::Warning);
            box.setWindowTitle(QObject::tr("Aviso"));
            box.addButton(QObject::tr("Aceptar"), QMessageBox::AcceptRole);
            break;
        case Kind::Error:
            box.setIcon(QMessageBox::Critical);
            box.setWindowTitle(QObject::tr("Error"));
            box.addButton(QObject::tr("Aceptar"), QMessageBox::AcceptRole);
            break;
        case Kind::Question: {
            box.setIcon(QMessageBox::Question);
            box.setWindowTitle(QObject::tr("Pregunta"));
            QPushButton* yes = box.addButton(QObject::tr("Sí"), QMessageBox::YesRole);
            box.addButton(QObject::tr("No"), QMessageBox::NoRole);
            box.setDefaultButton(yes);
            box.exec();
            return box.clickedButton() == yes;
        }
        case Kind::QuestionDefaultNo: {  // dlgavis_preguntar_no_por_defecto
            box.setIcon(QMessageBox::Question);
            box.setWindowTitle(QObject::tr("Pregunta"));
            QPushButton* yes = box.addButton(QObject::tr("Sí"), QMessageBox::YesRole);
            QPushButton* no = box.addButton(QObject::tr("No"), QMessageBox::NoRole);
            box.setDefaultButton(no);
            box.exec();
            return box.clickedButton() == yes;
        }
    }
    box.exec();
    return true;
}

}  // namespace cc::app::messages
