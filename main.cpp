#include <pybind11/embed.h>
#include <pybind11/stl.h>

#define QT_NO_KEYWORDS

#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QScrollArea>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QDir>
#include <QFileInfo>
#include <QSplitter>

namespace py = pybind11;


class DynamicFormWidget : public QWidget {
    Q_OBJECT
public:
    explicit DynamicFormWidget(QWidget* parent = nullptr)
        : QWidget(parent), m_layout(nullptr) {}

    struct FieldEntry {
        QString name;
        QString type;
        QWidget* widget = nullptr;
        QLabel* label = nullptr;
        QLineEdit* fileEdit = nullptr;
        QString fileFilter;
    };

    using FieldMap = QHash<QString, FieldEntry>;

    void renderFromSchema(const QJsonObject& schema, py::object pyProxy, py::object pyFormInstance) {
        delete m_layout;
        m_layout = new QFormLayout(this);
        m_layout->setSpacing(12);
        m_layout->setContentsMargins(15, 15, 15, 15);
        m_layout->setLabelAlignment(Qt::AlignLeft);

        m_pyProxy = pyProxy;
        m_pyFormInstance = pyFormInstance;

        QJsonArray fields = schema["fields"].toArray();
        QList<FieldEntry> entries;

        for (const auto& val : fields) {
            QJsonObject field = val.toObject();
            FieldEntry e;
            e.name = field["name"].toString();
            e.type = field["field_type"].toString();

            if (e.type == "bool") {
                auto chk = new QCheckBox(this);
                chk->setChecked(field["default"].toBool(false));
                e.widget = chk;
            } else if (e.type == "string") {
                auto edit = new QLineEdit(this);
                edit->setText(field["default"].toString());
                e.widget = edit;
            } else if (e.type == "int") {
                auto spin = new QSpinBox(this);
                spin->setRange(field["min"].toInt(0), field["max"].toInt(100));
                spin->setValue(field["default"].toInt(0));
                e.widget = spin;
            } else if (e.type == "enum") {
                auto combo = new QComboBox(this);
                for (const auto& opt : field["options"].toArray()) {
                    combo->addItem(opt.toString());
                }
                combo->setCurrentText(field["default"].toString());
                e.widget = combo;
            } else if (e.type == "file") {
                auto container = new QWidget(this);
                auto hbox = new QHBoxLayout(container);
                hbox->setContentsMargins(0, 0, 0, 0);
                auto edit = new QLineEdit(container);
                auto btn = new QPushButton("浏览...", container);
                hbox->addWidget(edit);
                hbox->addWidget(btn);

                QString filter = field["file_filter"].toString("All Files (*)");
                QObject::connect(btn, &QPushButton::clicked, this, [this, edit, filter]() {
                    QString path = QFileDialog::getOpenFileName(this, "选择文件", "", filter);
                    if (!path.isEmpty()) edit->setText(path);
                });

                e.widget = container;
                e.fileEdit = edit;
            }

            if (e.widget) {
                e.label = new QLabel(field["label"].toString(), this);
                m_layout->addRow(e.label, e.widget);
                m_fields[e.name] = e;
            }
            entries.append(e);
        }

        for (const auto& e : entries) {
            if (!e.widget) continue;
            if (e.type == "bool") {
                QObject::connect(qobject_cast<QCheckBox*>(e.widget), &QCheckBox::toggled, this, [this, name = e.name](bool checked) {
                    dispatchChange(name, py::bool_(checked));
                });
            } else if (e.type == "string") {
                QObject::connect(qobject_cast<QLineEdit*>(e.widget), &QLineEdit::textChanged, this, [this, name = e.name](const QString& text) {
                    dispatchChange(name, py::str(text.toStdString()));
                });
            } else if (e.type == "int") {
                QObject::connect(qobject_cast<QSpinBox*>(e.widget), QOverload<int>::of(&QSpinBox::valueChanged), this, [this, name = e.name](int val) {
                    dispatchChange(name, py::int_(val));
                });
            } else if (e.type == "enum") {
                QObject::connect(qobject_cast<QComboBox*>(e.widget), &QComboBox::currentTextChanged, this, [this, name = e.name](const QString& text) {
                    dispatchChange(name, py::str(text.toStdString()));
                });
            } else if (e.type == "file" && e.fileEdit) {
                QObject::connect(e.fileEdit, &QLineEdit::textChanged, this, [this, name = e.name](const QString& text) {
                    dispatchChange(name, py::str(text.toStdString()));
                });
            }
        }
    }

    QJsonObject collectValues() {
        QJsonObject result;
        for (auto it = m_fields.begin(); it != m_fields.end(); ++it) {
            const auto& e = it.value();
            if (e.type == "bool") {
                result[e.name] = qobject_cast<QCheckBox*>(e.widget)->isChecked();
            } else if (e.type == "string") {
                result[e.name] = qobject_cast<QLineEdit*>(e.widget)->text();
            } else if (e.type == "int") {
                result[e.name] = qobject_cast<QSpinBox*>(e.widget)->value();
            } else if (e.type == "enum") {
                result[e.name] = qobject_cast<QComboBox*>(e.widget)->currentText();
            } else if (e.type == "file") {
                result[e.name] = e.fileEdit->text();
            }
        }
        return result;
    }

    void applyProxyCommands() {
        py::list cmds = m_pyProxy.attr("commands");
        for (const auto& cmd : cmds) {
            std::string field = cmd.attr("__getitem__")(0).cast<std::string>();
            std::string prop  = cmd.attr("__getitem__")(1).cast<std::string>();
            py::object val    = cmd.attr("__getitem__")(2);

            auto it = m_fields.find(QString::fromStdString(field));
            if (it == m_fields.end()) continue;
            auto& e = it.value();

            if (prop == "visible") {
                bool v = val.cast<bool>();
                e.widget->setVisible(v);
                if (e.label) e.label->setVisible(v);
            } else if (prop == "enabled") {
                e.widget->setEnabled(val.cast<bool>());
            } else if (prop == "value") {
                if (auto spin = qobject_cast<QSpinBox*>(e.widget))
                    spin->setValue(val.cast<int>());
                else if (auto edit = qobject_cast<QLineEdit*>(e.widget))
                    edit->setText(QString::fromStdString(val.cast<std::string>()));
                else if (auto combo = qobject_cast<QComboBox*>(e.widget))
                    combo->setCurrentText(QString::fromStdString(val.cast<std::string>()));
                else if (auto chk = qobject_cast<QCheckBox*>(e.widget))
                    chk->setChecked(val.cast<bool>());
            }
        }
    }

    void dispatchChange(const QString& name, py::object value) {
        if (!m_pyFormInstance || !m_pyProxy) return;
        try {
            m_pyProxy.attr("clear")();
            m_pyFormInstance.attr("dispatch_change")(name.toStdString(), value, m_pyProxy);
            applyProxyCommands();
        } catch (py::error_already_set& e) {
            qWarning() << "dispatch_change error:" << e.what();
            e.restore();
        }
    }

    QFormLayout* m_layout = nullptr;
    py::object m_pyProxy;
    py::object m_pyFormInstance;
    FieldMap m_fields;
};


class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setWindowTitle("Qt + Python 动态表单系统");
        resize(950, 550);

        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);

        auto* vSplitter = new QSplitter(Qt::Vertical, this);

        auto* hSplitter = new QSplitter(Qt::Horizontal, this);
        hSplitter->setChildrenCollapsible(false);

        m_taskList = new QListWidget(this);
        m_taskList->setFixedWidth(180);

        auto* formScroll = new QScrollArea(this);
        formScroll->setWidgetResizable(true);
        m_stack = new QStackedWidget;
        formScroll->setWidget(m_stack);

        m_infoPanel = new QWidget(this);
        m_infoPanel->setMinimumWidth(200);
        auto* infoLayout = new QVBoxLayout(m_infoPanel);
        infoLayout->setContentsMargins(10, 15, 10, 10);

        m_infoTitle = new QLabel(m_infoPanel);
        m_infoTitle->setStyleSheet("font-size: 16px; font-weight: bold;");
        m_infoTitle->setWordWrap(true);

        auto* separator = new QFrame(m_infoPanel);
        separator->setFrameShape(QFrame::HLine);
        separator->setFrameShadow(QFrame::Sunken);

        m_infoDescription = new QLabel(m_infoPanel);
        m_infoDescription->setWordWrap(true);
        m_infoDescription->setStyleSheet("color: #444;");

        m_infoSchematic = new QLabel(m_infoPanel);
        m_infoSchematic->setAlignment(Qt::AlignCenter);

        infoLayout->addWidget(m_infoTitle);
        infoLayout->addWidget(separator);
        infoLayout->addSpacing(8);
        infoLayout->addWidget(m_infoDescription);
        infoLayout->addSpacing(16);
        infoLayout->addWidget(m_infoSchematic, 0, Qt::AlignHCenter);
        infoLayout->addStretch();

        hSplitter->addWidget(m_taskList);
        hSplitter->addWidget(formScroll);
        hSplitter->addWidget(m_infoPanel);
        hSplitter->setStretchFactor(0, 0);
        hSplitter->setStretchFactor(1, 1);
        hSplitter->setStretchFactor(2, 0);
        hSplitter->setSizes({180, 450, 300});

        m_infoPanel->setVisible(false);

        auto* bottomPanel = new QWidget(this);
        auto* bottomLayout = new QVBoxLayout(bottomPanel);
        bottomLayout->setContentsMargins(10, 8, 10, 8);
        m_submitBtn = new QPushButton("提交配置", bottomPanel);
        m_resultLabel = new QLabel("等待提交...", bottomPanel);
        m_resultLabel->setWordWrap(true);
        bottomLayout->addWidget(m_submitBtn);
        bottomLayout->addWidget(m_resultLabel);

        vSplitter->addWidget(hSplitter);
        vSplitter->addWidget(bottomPanel);
        vSplitter->setStretchFactor(0, 4);
        vSplitter->setStretchFactor(1, 1);
        vSplitter->setSizes({440, 110});

        mainLayout->addWidget(vSplitter);

        setupPythonProxy();
        discoverTasks();

        QObject::connect(m_taskList, &QListWidget::currentRowChanged, this, &MainWindow::onTaskSelected);
        QObject::connect(m_submitBtn, &QPushButton::clicked, this, &MainWindow::onSubmit);

        if (m_taskList->count() > 0) {
            m_taskList->setCurrentRow(0);
        }
    }

private:
    void setupPythonProxy() {
        py::exec(R"(
class _ContextProxy:
    def __init__(self):
        self.commands = []
    def set_widget_property(self, field_name, prop, value):
        self.commands.append((field_name, prop, value))
    def clear(self):
        self.commands = []
)");
        m_pyProxyTemplate = py::eval("_ContextProxy");
    }

    DynamicFormWidget* currentForm() {
        return qobject_cast<DynamicFormWidget*>(m_stack->currentWidget());
    }

    void discoverTasks() {
        QSet<QString> seen;
        for (const QString& dir : {
            QApplication::applicationDirPath() + "/tasks",
            QDir::currentPath() + "/tasks",
            QStringLiteral("/home/linfeng/code/qt_py/tasks"),
        }) {
            QDir tasksDir(dir);
            if (!tasksDir.exists()) continue;
            for (const QFileInfo& info : tasksDir.entryInfoList({"*.py"}, QDir::Files)) {
                if (info.baseName() == "__init__") continue;
                if (seen.contains(info.baseName())) continue;
                seen.insert(info.baseName());
                QString modulePath = "tasks." + info.baseName();
                auto* item = new QListWidgetItem(info.baseName(), m_taskList);
                item->setData(Qt::UserRole, modulePath);
            }
        }
    }

    void onTaskSelected(int index) {
        if (index < 0) return;
        QString modulePath = m_taskList->item(index)->data(Qt::UserRole).toString();

        if (m_taskPages.contains(modulePath)) {
            m_stack->setCurrentIndex(m_taskPages[modulePath]);
            populateInfoPanel(modulePath);
            return;
        }

        auto* page = new DynamicFormWidget;
        loadTask(page, modulePath);
        m_stack->addWidget(page);
        m_taskPages[modulePath] = m_stack->count() - 1;
        m_stack->setCurrentWidget(page);
    }

    void loadTask(DynamicFormWidget* page, const QString& modulePath) {
        try {
            py::object module = py::module_::import(modulePath.toStdString().c_str());
            py::object formInst = module.attr("get_form_instance")();

            py::dict meta = formInst.attr("export_meta")().cast<py::dict>();
            py::module_ json_mod = py::module_::import("json");
            std::string jsonStr = json_mod.attr("dumps")(meta, py::arg("ensure_ascii") = false).cast<std::string>();
            QJsonParseError parseErr;
            QJsonDocument doc = QJsonDocument::fromJson(
                QString::fromStdString(jsonStr).toUtf8(), &parseErr
            );
            if (doc.isNull()) {
                throw std::runtime_error("JSON parse error: " + parseErr.errorString().toStdString()
                    + "\nraw: " + jsonStr.substr(0, 200));
            }
            QJsonObject schema = doc.object();
            m_taskSchemas[modulePath] = schema;

            py::object proxy = m_pyProxyTemplate();
            page->renderFromSchema(schema, proxy, formInst);

            for (const auto& val : schema["fields"].toArray()) {
                QJsonObject field = val.toObject();
                QString name = field["name"].toString();
                QJsonValue def = field["default"];
                py::object pyDef;
                if (def.isBool()) pyDef = py::bool_(def.toBool());
                else if (def.isDouble()) pyDef = py::int_(def.toInt());
                else pyDef = py::str(def.toString().toStdString());

                proxy.attr("clear")();
                formInst.attr("dispatch_change")(name.toStdString(), pyDef, proxy);
                page->applyProxyCommands();
            }

            populateInfoPanel(modulePath);

        } catch (py::error_already_set& e) {
            qWarning() << "Failed to load task:" << e.what();
            QMessageBox::warning(this, "错误",
                QString("加载任务失败: %1").arg(e.what()));
            e.restore();
        } catch (std::exception& e) {
            qWarning() << "Failed to load task:" << e.what();
            QMessageBox::warning(this, "错误",
                QString("加载任务失败: %1").arg(e.what()));
        }
    }

    void populateInfoPanel(const QString& modulePath) {
        if (!m_taskSchemas.contains(modulePath)) return;
        const QJsonObject& schema = m_taskSchemas[modulePath];

        m_infoTitle->setText(schema["type"].toString());
        m_infoDescription->setText(schema["description"].toString("暂无说明"));

        QString schematicPath = schema["schematic"].toString();
        QPixmap pix;
        if (!schematicPath.isEmpty()) {
            pix = QPixmap(schematicPath);
            if (pix.isNull()) {
                int idx = schematicPath.lastIndexOf("/tasks/");
                QString relative = (idx >= 0) ? schematicPath.mid(idx + 7) : schematicPath;
                for (const QString& base : {
                    QDir::currentPath(),
                    QStringLiteral("/home/linfeng/code/qt_py"),
                }) {
                    pix = QPixmap(base + "/" + relative);
                    if (!pix.isNull()) break;
                }
            }
        }

        if (!pix.isNull()) {
            int panelWidth = m_infoPanel->width() - 20;
            int maxW = qMin(400, panelWidth);
            m_infoSchematic->setPixmap(pix.scaledToWidth(maxW, Qt::SmoothTransformation));
            m_infoSchematic->setFixedHeight(m_infoSchematic->pixmap().height());
            m_infoSchematic->show();
        } else {
            m_infoSchematic->hide();
        }
        m_infoPanel->setVisible(true);
    }

    void onSubmit() {
        auto* form = currentForm();
        if (!form) return;
        QJsonObject values = form->collectValues();
        QString resultJson = QJsonDocument(values).toJson(QJsonDocument::Indented);
        m_resultLabel->setText("配置数据:\n" + resultJson);
    }

    QListWidget* m_taskList;
    QStackedWidget* m_stack;
    QHash<QString, int> m_taskPages;
    QHash<QString, QJsonObject> m_taskSchemas;
    QPushButton* m_submitBtn;
    QLabel* m_resultLabel;

    QWidget* m_infoPanel;
    QLabel* m_infoTitle;
    QLabel* m_infoDescription;
    QLabel* m_infoSchematic;

    py::object m_pyProxyTemplate;
};


int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    py::scoped_interpreter guard{};

    py::module_ sys = py::module_::import("sys");
    sys.attr("path").attr("append")(QDir::currentPath().toStdString());
    sys.attr("path").attr("append")(QApplication::applicationDirPath().toStdString());
    sys.attr("path").attr("append")("/home/linfeng/code/qt_py");

    MainWindow window;
    window.show();

    return app.exec();
}


#include "main.moc"
