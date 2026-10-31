#pragma once
#include <QIcon>
#include <QString>

namespace cc::app {

/// Returns the bundled SVG icon called `name` (resources/icons/<name>.svg).
/// All icons are original artwork; none are copied from the CYPE application.
inline QIcon themedIcon(const QString& name) { return QIcon(QStringLiteral(":/icons/%1.svg").arg(name)); }

}  // namespace cc::app

