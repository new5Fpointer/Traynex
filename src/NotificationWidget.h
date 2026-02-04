#pragma once

#include <QWidget>
#include <QTimer>
#include <QPropertyAnimation>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>

class NotificationWidget : public QWidget {
	Q_OBJECT
		Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
	// 添加通知类型枚举
	enum NotificationType {
		Info,      // 信息 - 蓝色
		Success,   // 成功 - 绿色  
		Warning,   // 警告 - 黄色
		Error      // 错误 - 红色
	};

	explicit NotificationWidget(QWidget* parent = nullptr);
	~NotificationWidget() override = default;

	void showNotification(const QString& title, const QString& message,
		NotificationType type = Info, int duration = 3000);
	void hideNotification();
	NotificationType type() const { return m_type; }

protected:
	void paintEvent(QPaintEvent* event) override;
	void enterEvent(QEnterEvent* event) override;
	void leaveEvent(QEvent* event) override;

private slots:
	void onCloseClicked();

private:
	void setupUI();
	void setupAnimations();
	void calculatePosition();

	qreal opacity() const { return m_opacity; }
	void setOpacity(qreal opacity);

	// 根据类型获取颜色
	QColor getColorForType(NotificationType type) const;

	// UI组件
	QLabel* m_titleLabel = nullptr;
	QLabel* m_messageLabel = nullptr;
	QPushButton* m_closeButton = nullptr;

	// 动画和定时器
	QTimer* m_timer = nullptr;
	QPropertyAnimation* m_showAnimation = nullptr;
	QPropertyAnimation* m_hideAnimation = nullptr;

	// 属性
	qreal m_opacity;
	bool m_isHovered;
	NotificationType m_type;

	// 父窗口引用
	QWidget* m_parentWidget = nullptr;
};