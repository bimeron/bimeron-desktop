#pragma once
#include "NativeIKEv2Session.h"
#include <QPointer>
#include <QJsonArray>
#include <QRegularExpression>

// Local consent boundary only. Production has no authority provider and therefore
// no controller. Session remains the sole owner of transport and lifecycle rules.
class NativeIKEv2Actions final : public QObject {
public:
    enum class State { Unavailable, Unknown, Previewing, Reviewable, Applying, Succeeded, Stale, Failed };
    bool provisioning() const { return endpoint_ == NativeIKEv2Session::Endpoint::Provisioning; }
    State state() const {
        if (!available()) return bound_ ? State::Unknown : State::Unavailable;
        if (!snapshot_.authoritative) return State::Unknown;
        if (snapshot_.safeError == "ikev2_profile_preview_changed") return State::Stale;
        if (!snapshot_.safeError.isEmpty() || snapshot_.recoveryRequired || snapshot_.operation == "failed") return State::Failed;
        if (snapshot_.operation == "previewing") return State::Previewing;
        if (review().generation) return State::Reviewable;
        if (snapshot_.operation == "applying") return State::Applying;
        if (snapshot_.operation == "succeeded") return State::Succeeded;
        return State::Unknown;
    }
    struct Consent {
        quint64 generation = 0;
        QString profile, server, action, instance, token;
        QJsonObject plan;
    };
    explicit NativeIKEv2Actions(QObject *parent = nullptr) : QObject(parent) {}
    bool available() const { return !controller_.isNull(); }
    NativeIKEv2Session::Snapshot snapshot() const { return snapshot_; }
    void select(const QString &profile, const QString &server, const QString &action) {
        invalidate(); profile_ = profile; server_ = server; action_ = action;
    }
    bool preview() {
        invalidate();
        if (!controller_ || profile_.isEmpty() || server_.isEmpty()
            || (action_ != "apply" && action_ != "remove")) return false;
        endpoint_ = NativeIKEv2Session::Endpoint::Provisioning;
        active_ = true;
        if (controller_->previewProfile(profile_, server_, action_)) return true;
        invalidate(); return false;
    }
    Consent review() const {
        if (!active_ || !controller_ || !snapshot_.authoritative
            || !snapshot_.safeError.isEmpty() || snapshot_.recoveryRequired
            || snapshot_.closureAccepted || snapshot_.ownerClosed
            || snapshot_.operation != "previewed") return {};
        const auto p = snapshot_.profile;
        if (!boundPlan(p)
            || snapshot_.instance.isEmpty()) return {};
        return {generation_, profile_, server_, action_, snapshot_.instance, p.value("state_token").toString(), p};
    }
    bool confirm(const Consent &consent) {
        const auto current = review();
        if (!current.generation || consent.generation != current.generation
            || consent.profile != current.profile || consent.server != current.server
            || consent.action != current.action || consent.instance != current.instance
            || consent.token != current.token || consent.plan != current.plan) return false;
        invalidate(); // Consume before dispatch, including synchronous callbacks.
        return controller_ && controller_->confirmPreview(consent.token);
    }
    void cancel() { invalidate(); if (controller_) controller_->cancelExchange(); }
    bool stop() { invalidate(); return controller_ && controller_->stop(); }
    bool rediscover() { invalidate(); return controller_ && controller_->discover(endpoint_); }
    bool closeOwner() { invalidate(); return controller_ && controller_->closeOwner(); }
private:
    bool boundPlan(const QJsonObject &p) const {
        const auto action = p.value("action").toString();
        return validPlan(p) && p.value("windows_profile") == profile_
            && p.value("server_name") == server_
            && (action_ == "remove" ? action == "remove" : action == "create" || action == "unchanged");
    }
    static bool validPlan(const QJsonObject &p) {
        const auto bounded = [](const QJsonValue &v, int limit) {
            return v.isString() && !v.toString().trimmed().isEmpty() && v.toString().size() <= limit;
        };
        if (!bounded(p.value("windows_profile"), 256) || !bounded(p.value("server_name"), 253)
            || !p.value("action").isString()
            || (p.value("action") != "create" && p.value("action") != "unchanged" && p.value("action") != "remove")
            || !p.value("state_token").isString()
            || !QRegularExpression("^[0-9a-f]{64}$").match(p.value("state_token").toString()).hasMatch()) return false;
        for (const auto *key : {"authentication", "ipsec", "credentials", "trust"})
            if (!bounded(p.value(key), 2048)) return false;
        if (!p.value("ipv6_routes").isArray()) return false;
        const auto routes = p.value("ipv6_routes").toArray();
        if (routes.size() > 16) return false;
        for (const auto &route : routes) if (!bounded(route, 2048)) return false;
        return true;
    }
    void invalidate() { ++generation_; active_ = false; }
    void bind(NativeIKEv2Session *controller) {
        if (!controller || !controller->fake_ || bound_) return;
        bound_ = true;
        controller_ = controller;
        snapshot_ = controller->snapshot();
        connect(controller, &NativeIKEv2Session::changed, this, [this] {
            const auto next = controller_->snapshot();
            if ((next.operation == "previewed" && !boundPlan(next.profile))
                || (review().generation && (next.profile != snapshot_.profile || next.operation != "previewed"))
                || !next.authoritative || !next.safeError.isEmpty() || next.recoveryRequired
                || next.closureAccepted || next.ownerClosed || next.operation == "failed"
                || (!snapshot_.instance.isEmpty() && next.instance != snapshot_.instance)
                || (!snapshot_.profile.value("state_token").toString().isEmpty()
                    && next.profile.value("state_token") != snapshot_.profile.value("state_token"))) invalidate();
            if (provisioning() && next.authoritative) {
                if (!next.safeError.isEmpty()) provisioningError_ = next.safeError;
                provisioningRecovery_ |= next.recoveryRequired;
            }
            snapshot_ = next;
            if (provisioning()) {
                if (!provisioningError_.isEmpty()) snapshot_.safeError = provisioningError_;
                snapshot_.recoveryRequired |= provisioningRecovery_;
            }
        });
        connect(controller, &QObject::destroyed, this, [this] {
            invalidate(); controller_.clear(); snapshot_.authoritative = false;
            snapshot_.ownerClosed = false; snapshot_.recoveryRequired = true;
        });
    }
    NativeIKEv2Session::Endpoint endpoint_ = NativeIKEv2Session::Endpoint::Session;
    QPointer<NativeIKEv2Session> controller_;
    NativeIKEv2Session::Snapshot snapshot_;
    QString profile_, server_, action_, provisioningError_;
    bool provisioningRecovery_ = false;
    quint64 generation_ = 1;
    bool active_ = false;
    bool bound_ = false;
    friend class MainWindow;
    friend struct NativeIKEv2SessionTest;
};
