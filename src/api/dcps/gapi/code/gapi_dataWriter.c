/*
 *                         OpenSplice DDS
 *
 *   This software and documentation are Copyright 2006 to 2009 PrismTech
 *   Limited and its licensees. All rights reserved. See file:
 *
 *                     $OSPL_HOME/LICENSE
 *
 *   for full copyright notice and license terms.
 *
 */
#include "gapi_dataWriter.h"
#include "gapi_qos.h"
#include "gapi_publisher.h"
#include "gapi_dataWriterStatus.h"
#include "gapi_structured.h"
#include "gapi_typeSupport.h"
#include "gapi_topicDescription.h"
#include "gapi_topic.h"
#include "gapi_kernel.h"
#include "gapi_objManag.h"
#include "gapi_genericCopyIn.h"
#include "gapi_genericCopyOut.h"
#include "gapi_error.h"

#include "os_heap.h"
#include "os_stdlib.h"
#include "v_kernel.h"

#define _GAPICC_

static gapi_boolean
copyWriterQosIn (
    const gapi_dataWriterQos *srcQos,
    v_writerQos dstQos)
{
    gapi_boolean copied = FALSE;

    /* Important note: The sequence related to policies is created on
     * heap and will be copied into the database by the kernel itself,
     * therefor the c_array does not need to be allocated with c_arrayNew,
     * but can be allocated on heap.
     */

    if (dstQos->userData.value) {
        os_free(dstQos->userData.value);
        dstQos->userData.value = NULL;
    }
    dstQos->userData.size = srcQos->user_data.value._length;
    if (dstQos->userData.size) {
        dstQos->userData.value = os_malloc (dstQos->userData.size);
    }
    if ((srcQos->user_data.value._length == 0) || dstQos->userData.value) {
        kernelCopyInDuration(&srcQos->deadline.period, &dstQos->deadline.period);
        dstQos->durability.kind = srcQos->durability.kind;
        dstQos->history.kind = srcQos->history.kind;
        dstQos->history.depth = srcQos->history.depth;
        kernelCopyInDuration(&srcQos->latency_budget.duration,
                             &dstQos->latency.duration);
        dstQos->liveliness.kind = srcQos->liveliness.kind;
        kernelCopyInDuration(&srcQos->liveliness.lease_duration,
                             &dstQos->liveliness.lease_duration);
        dstQos->orderby.kind = srcQos->destination_order.kind;
        dstQos->reliability.kind = srcQos->reliability.kind;
        kernelCopyInDuration(&srcQos->reliability.max_blocking_time,
                             &dstQos->reliability.max_blocking_time);
        dstQos->reliability.synchronous = srcQos->reliability.synchronous;
        dstQos->resource.max_samples = srcQos->resource_limits.max_samples;
        dstQos->resource.max_instances = srcQos->resource_limits.max_instances;
        dstQos->resource.max_samples_per_instance =
                     srcQos->resource_limits.max_samples_per_instance;
        dstQos->ownership.kind = srcQos->ownership.kind;
        dstQos->strength.value = srcQos->ownership_strength.value;
        dstQos->transport.value = srcQos->transport_priority.value;
        kernelCopyInDuration(&srcQos->lifespan.duration,
                             &dstQos->lifespan.duration);
        if (srcQos->user_data.value._length) {
            memcpy (dstQos->userData.value,
                    srcQos->user_data.value._buffer,
                    srcQos->user_data.value._length);
        }
        dstQos->lifecycle.autodispose_unregistered_instances =
            srcQos->writer_data_lifecycle.autodispose_unregistered_instances;
        kernelCopyInDuration(&srcQos->writer_data_lifecycle.autopurge_suspended_samples_delay,
                             &dstQos->lifecycle.autopurge_suspended_samples_delay);
        kernelCopyInDuration(&srcQos->writer_data_lifecycle.autounregister_instance_delay,
                             &dstQos->lifecycle.autounregister_instance_delay);
        copied = TRUE;
    }

    return copied;
}

static gapi_boolean
copyWriterQosOut (
    const v_writerQos  srcQos,
    gapi_dataWriterQos *dstQos)
{
    assert(srcQos);
    assert(dstQos);

    kernelCopyOutDuration(&srcQos->deadline.period, &dstQos->deadline.period);
    dstQos->durability.kind = srcQos->durability.kind;

    dstQos->history.kind = srcQos->history.kind;
    dstQos->history.depth = srcQos->history.depth;
    kernelCopyOutDuration(&srcQos->latency.duration,
                          &dstQos->latency_budget.duration);
    kernelCopyOutDuration(&srcQos->lifespan.duration,
                          &dstQos->lifespan.duration);
    dstQos->liveliness.kind = srcQos->liveliness.kind;
    kernelCopyOutDuration(&srcQos->liveliness.lease_duration,
                          &dstQos->liveliness.lease_duration);
    dstQos->destination_order.kind = srcQos->orderby.kind;
    dstQos->reliability.kind = srcQos->reliability.kind;
    kernelCopyOutDuration(&srcQos->reliability.max_blocking_time,
                          &dstQos->reliability.max_blocking_time);
    dstQos->reliability.synchronous = srcQos->reliability.synchronous;
    dstQos->resource_limits.max_samples = srcQos->resource.max_samples;
    dstQos->resource_limits.max_instances = srcQos->resource.max_instances;
    dstQos->resource_limits.max_samples_per_instance = srcQos->resource.max_samples_per_instance;
    dstQos->transport_priority.value = srcQos->transport.value;
    dstQos->ownership.kind = srcQos->ownership.kind;
    dstQos->ownership_strength.value = srcQos->strength.value;
    dstQos->writer_data_lifecycle.autodispose_unregistered_instances = srcQos->lifecycle.autodispose_unregistered_instances;
    kernelCopyOutDuration(&srcQos->lifecycle.autopurge_suspended_samples_delay,
                          &dstQos->writer_data_lifecycle.autopurge_suspended_samples_delay);
    kernelCopyOutDuration(&srcQos->lifecycle.autounregister_instance_delay,
                          &dstQos->writer_data_lifecycle.autounregister_instance_delay);

    if ( dstQos->user_data.value._maximum > 0 ) {
        if ( dstQos->user_data.value._release ) {
            gapi_free(dstQos->user_data.value._buffer);
        }
    }

    if ( (srcQos->userData.size > 0) && srcQos->userData.value ) {
        dstQos->user_data.value._buffer = gapi_octetSeq_allocbuf(srcQos->userData.size);
        if ( dstQos->user_data.value._buffer ) {
            dstQos->user_data.value._maximum = srcQos->userData.size;
            dstQos->user_data.value._length  = srcQos->userData.size;
            dstQos->user_data.value._release = TRUE;
            memcpy(dstQos->user_data.value._buffer,
                   srcQos->userData.value,
                   srcQos->userData.size);
        }
    } else {
            dstQos->user_data.value._maximum = 0;
            dstQos->user_data.value._length  = 0;
            dstQos->user_data.value._release = FALSE;
            dstQos->user_data.value._buffer = NULL;
    }

    return TRUE;
}
static c_bool
_DataWriterCopy (
    c_type type,
    void *data,
    void *to)
{
    c_bool result;
    c_base base = c_getBase(c_object(type));
    writerInfo *info = (writerInfo *)data;

    if (info->writer->copy_cache) {
        C_STRUCT(gapi_srcInfo) dataInfo;
        dataInfo.copyProgram = info->writer->copy_cache;
        dataInfo.src = info->data;
        result = info->writer->copy_in (base, &dataInfo, to);
    } else {
        result = info->writer->copy_in (base, info->data, to);
    }
    return result;
}

_DataWriter
_DataWriterNew (
    const _Topic topic,
    const _TypeSupport typesupport,
    const gapi_dataWriterQos *qos,
    const struct gapi_dataWriterListener *a_listener,
    const gapi_statusMask mask,
    const _Publisher publisher)
{
    _DataWriter newDataWriter;
    v_writerQos writerQos;
    u_writer uWriter;
    _TypeSupport typeSupport = (_TypeSupport)typesupport;
    char dataWriterId[256];
    gapi_string topicName;

    newDataWriter = _DataWriterAlloc();

    if ( newDataWriter != NULL ) {
        _DomainEntityInit(_DomainEntity(newDataWriter),
                          _DomainEntityParticipant(_DomainEntity(publisher)),
                          _Entity(publisher),
                          FALSE);

        newDataWriter->topic = topic;
        if ( a_listener ) {
            newDataWriter->listener = *a_listener;
        }
        newDataWriter->publisher = publisher;
        writerQos = u_writerQosNew(NULL);
        if ( (writerQos != NULL) ) {
            if ( !copyWriterQosIn(qos, writerQos) ) {
                u_writerQosFree (writerQos);
                _DomainEntityDispose(_DomainEntity(newDataWriter));
                newDataWriter = NULL;
            }
        } else {
            _DomainEntityDispose(_DomainEntity(newDataWriter));
            newDataWriter = NULL;
        }
    }

    if ( newDataWriter != NULL ) {
        if ( _TypeSupportGetWriterCopy(typeSupport) ) {
            newDataWriter->uWriterCopy = _TypeSupportGetWriterCopy(typeSupport);
        } else {
            newDataWriter->uWriterCopy = _DataWriterCopy;
        }
        newDataWriter->copy_in    = _TypeSupportCopyIn(typeSupport);
        newDataWriter->copy_out   = _TypeSupportCopyOut(typeSupport);
        newDataWriter->copy_cache = _TypeSupportCopyCache(typeSupport);

        topicName = _TopicDescriptionGetName (_TopicDescription(topic));
        if (topicName) {
            snprintf (dataWriterId, sizeof (dataWriterId), "%sWriter", topicName);
            gapi_free (topicName);
        } else {
            snprintf (dataWriterId, sizeof (dataWriterId), "dataWriter");
        }
        uWriter = u_writerNew(_PublisherUpublisher(publisher),
                              dataWriterId,
                              _TopicUtopic(topic),
                              newDataWriter->uWriterCopy,
                              writerQos,
                              FALSE);
        if ( uWriter != NULL ) {
            U_WRITER_SET(newDataWriter, uWriter);
        } else {
            _DomainEntityDispose(_DomainEntity(newDataWriter));
            newDataWriter = NULL;
        }
        u_writerQosFree(writerQos);
    }

    if ( newDataWriter != NULL ) {
       _EntityStatus(newDataWriter) = _Status(_DataWriterStatusNew(newDataWriter,
                                              a_listener,mask));
        if ( _EntityStatus(newDataWriter) != NULL ) {
            _TopicDescriptionIncUse(_TopicDescription(topic));
        } else {
            u_writerFree(U_WRITER_GET(uWriter));
            _DomainEntityDispose(_DomainEntity(newDataWriter));
            newDataWriter = NULL;
        }
    }

    return newDataWriter;
}


void
_DataWriterFree (
    _DataWriter _this)
{
    assert(_this);

    _TopicDescriptionDecUse(_TopicDescription(_this->topic));

    _DataWriterStatusSetListener(_DataWriterStatus(_Entity(_this)->status), NULL, 0);

    _DataWriterStatusFree(_DataWriterStatus(_Entity(_this)->status));

    _EntityFreeStatusCondition(_Entity(_this));

    u_writerFree(U_WRITER_GET(_this));

    if ( _this->registry ) {
        gapi_hashTableFree(_this->registry);
    }

    _DomainEntityDispose (_DomainEntity(_this));
}

gapi_boolean
_DataWriterPrepareDelete (
    _DataWriter _this)
{
    return TRUE;
}

gapi_dataWriterQos *
_DataWriterGetQos (
    _DataWriter dataWriter,
    gapi_dataWriterQos * qos)
{
    v_writerQos dataWriterQos;
    u_writer uWriter;

    assert(dataWriter);

    uWriter = U_WRITER_GET(dataWriter);

    if ( u_entityQoS(u_entity(uWriter), (v_qos*)&dataWriterQos) == U_RESULT_OK ) {
        copyWriterQosOut(dataWriterQos,  qos);
        u_writerQosFree(dataWriterQos);
    }

    return qos;
}

/*     ReturnCode_t
 *     set_qos(
 *         in DataWriterQos qos);
 *
 * Function will operate independent of the enable flag
 */
gapi_returnCode_t
gapi_dataWriter_set_qos (
    gapi_dataWriter _this,
    const gapi_dataWriterQos *qos)
{
    gapi_returnCode_t result = GAPI_RETCODE_OK;
    u_result uResult;
    _DataWriter dataWriter;
    v_writerQos dataWriterQos;
    gapi_context context;

    GAPI_CONTEXT_SET(context, _this, GAPI_METHOD_SET_QOS);

    dataWriter = gapi_dataWriterClaim(_this, &result);

    if ( dataWriter != NULL ) {
        result = gapi_dataWriterQosIsConsistent(qos, &context);
    } else {
        result = GAPI_RETCODE_BAD_PARAMETER;
    }

    if (( result == GAPI_RETCODE_OK ) && (_Entity(dataWriter)->enabled)) {
        gapi_dataWriterQos *existing_qos = gapi_dataWriterQos__alloc();

        result = gapi_dataWriterQosCheckMutability(qos,
                                                   _DataWriterGetQos(dataWriter,
                                                                     existing_qos),
                                                   &context);
        gapi_free(existing_qos);
    }


    if ( result == GAPI_RETCODE_OK ) {
        dataWriterQos = u_writerQosNew(NULL);
        if (dataWriterQos) {
            if ( copyWriterQosIn(qos, dataWriterQos) ) {
                uResult = u_entitySetQoS(_EntityUEntity(dataWriter),
                                         (v_qos)(dataWriterQos) );
                result = kernelResultToApiResult(uResult);
                u_writerQosFree(dataWriterQos);
            } else {
                result = GAPI_RETCODE_OUT_OF_RESOURCES;
            }
        } else {
            result = GAPI_RETCODE_OUT_OF_RESOURCES;
        }
    }

    _EntityRelease(dataWriter);

    return result;
}

/*     ReturnCode_t
 *     get_qos(
 *         inout DataWriterQos qos);
 *
 * Function will operate indepenedent of the enable flag
 */
gapi_returnCode_t
gapi_dataWriter_get_qos (
    gapi_dataWriter _this,
    gapi_dataWriterQos *qos)
{
    _DataWriter dataWriter;
    gapi_returnCode_t result;

    dataWriter = gapi_dataWriterClaim(_this, &result);
    if ( dataWriter && qos ) {
        _DataWriterGetQos(dataWriter, qos);
    }

    _EntityRelease(dataWriter);
    return result;

}

/*     ReturnCode_t
 *     set_listener(
 *         in DataWriterListener a_listener,
 *         in StatusMask mask);
 */
gapi_returnCode_t
gapi_dataWriter_set_listener (
    gapi_dataWriter _this,
    const struct gapi_dataWriterListener *a_listener,
    const gapi_statusMask mask)
{
    gapi_returnCode_t result = GAPI_RETCODE_ERROR;
    _DataWriter datawriter;

    datawriter = gapi_dataWriterClaim(_this, &result);

    if ( datawriter != NULL ) {
        _DataWriterStatus status;

        if ( a_listener ) {
            datawriter->listener = *a_listener;
        } else {
            memset(&datawriter->listener, 0, sizeof(datawriter->listener));
        }

        status = _DataWriterStatus(_EntityStatus(datawriter));
        if ( _DataWriterStatusSetListener(status, a_listener, mask) ) {
            result = GAPI_RETCODE_OK;
        }
    }

    _EntityRelease(datawriter);

    return result;
}


/*     DataWriterListener
 *     get_listener();
 *
 * Function will operate indepenedent of the enable flag
 */
struct gapi_dataWriterListener
gapi_dataWriter_get_listener (
    gapi_dataWriter _this)
{
    _DataWriter datawriter;
    struct gapi_dataWriterListener listener;

    datawriter = gapi_dataWriterClaim(_this, NULL);

    if ( datawriter != NULL ) {
        listener = datawriter->listener;
    } else {
        memset(&listener, 0, sizeof(listener));
    }

   _EntityRelease(datawriter);

    return listener;
}

/*     Topic
 *     get_topic();
 */
gapi_topic
gapi_dataWriter_get_topic (
    gapi_dataWriter _this)
{
    _DataWriter datawriter;
    _Topic topic = NULL;

    datawriter = gapi_dataWriterClaim(_this, NULL);

    if ( datawriter != NULL ) {
        topic = (_Topic)datawriter->topic;
        _EntityClaim(topic);
    }

    _EntityRelease(datawriter);

    return (gapi_topic)_EntityRelease(topic);
}

#ifdef _NEWGAPI_
static u_result
_DataWriterGetPublisherAction (
    v_entity _this,
    c_voidp arg)
{
    c_voidp *publisher = (c_voidp *)arg;
    *publisher = v_entityGetUserData(v_writerPublisher(_this));
}

/*     Publisher
 *     get_publisher();
 */
gapi_publisher
gapi_dataWriter_get_publisher (
    gapi_dataWriter _this)
{
    _DataWriter datawriter;
    _Publisher publisher = NULL;

    datawriter = gapi_dataWriterClaim(_this, NULL);

    if ( datawriter != NULL ) {
        u_entityAction(U_ENTITY_GET(datawriter),
                       _DataWriterGetPublisherAction,
                       (c_voidp)&publisher);
        _EntityClaim(publisher);
    }

    _EntityRelease(datawriter);

    return (gapi_publisher)_EntityRelease(publisher);
}
#else
/*     Publisher
 *     get_publisher();
 */
gapi_publisher
gapi_dataWriter_get_publisher (
    gapi_dataWriter _this)
{
    _DataWriter datawriter;
    _Publisher publisher = NULL;

    datawriter = gapi_dataWriterClaim(_this, NULL);

    if ( datawriter != NULL ) {
        publisher = (_Publisher)datawriter->publisher;
        _EntityClaim(publisher);
    }

    _EntityRelease(datawriter);

    return (gapi_publisher)_EntityRelease(publisher);
}
#endif

gapi_returnCode_t
gapi_dataWriter_wait_for_acknowledgments (
	gapi_dataWriter _this,
    const gapi_duration_t *max_wait
    )
{
	_DataWriter datawriter;
	u_result uResult;
	gapi_returnCode_t result;
	c_time timeout;

	datawriter = gapi_dataWriterClaim(_this, NULL);

	if ( datawriter != NULL ) {
		kernelCopyInDuration(max_wait, &timeout);
		uResult = u_writerWaitForAcknowledgments(
					u_writer(_EntityUEntity(datawriter)),
					timeout);
		result = kernelResultToApiResult(uResult);
	} else {
		result = GAPI_RETCODE_BAD_PARAMETER;
	}
	_EntityRelease(datawriter);

	return result;
}

static v_result
copy_liveliness_lost_status(
    c_voidp info,
    c_voidp arg)
{
    struct v_livelinessLostInfo *from;
    gapi_livelinessLostStatus *to;

    from = (struct v_livelinessLostInfo *)info;
    to = (gapi_livelinessLostStatus *)arg;

    to->total_count = from->totalCount;
    to->total_count_change = from->totalChanged;
    return V_RESULT_OK;

}

gapi_returnCode_t
_DataWriter_get_liveliness_lost_status (
    _DataWriter _this,
    c_bool reset,
    gapi_livelinessLostStatus *status)
{
    u_result uResult;
    uResult = u_writerGetLivelinessLostStatus(
                  U_WRITER_GET(_this),
                  reset,
                  copy_liveliness_lost_status,
                  status);

    return kernelResultToApiResult(uResult);
}

gapi_returnCode_t
gapi_dataWriter_get_liveliness_lost_status (
    gapi_dataWriter _this,
    gapi_livelinessLostStatus *status)
{
    gapi_returnCode_t result;
    _DataWriter datawriter;

    datawriter = gapi_dataWriterClaim(_this, &result);

    if (datawriter != NULL) {
        if (_Entity(datawriter)->enabled ) {
            result = _DataWriter_get_liveliness_lost_status(
                         datawriter,
                         TRUE,
                         status);
        } else {
            result = GAPI_RETCODE_NOT_ENABLED;
        }
    }

    _EntityRelease(datawriter);

    return result;
}

static v_result
copy_deadline_missed_status(
    c_voidp info,
    c_voidp arg)
{
    struct v_deadlineMissedInfo *from;
    gapi_offeredDeadlineMissedStatus *to;
    v_handleResult result;
    v_public instance;

    from = (struct v_deadlineMissedInfo *)info;
    to = (gapi_offeredDeadlineMissedStatus *)arg;

    to->total_count = from->totalCount;
    to->total_count_change = from->totalChanged;

    result = v_handleClaim(from->instanceHandle, (v_object*)&instance);
    if (result == V_HANDLE_OK) {
        to->last_instance_handle = u_instanceHandleNew(v_public(instance));
        result = v_handleRelease(from->instanceHandle);
    }
    return V_RESULT_OK;
}

gapi_returnCode_t
_DataWriter_get_offered_deadline_missed_status (
    _DataWriter _this,
    c_bool reset,
    gapi_offeredDeadlineMissedStatus *status)
{
    u_result uResult;
    uResult = u_writerGetDeadlineMissedStatus(
                  U_WRITER_GET(_this),
                  reset,
                  copy_deadline_missed_status,
                  status);

    return kernelResultToApiResult(uResult);
}

gapi_returnCode_t
gapi_dataWriter_get_offered_deadline_missed_status (
    gapi_dataWriter _this,
    gapi_offeredDeadlineMissedStatus *status)
{
    gapi_returnCode_t result;
    _DataWriter datawriter;


    datawriter = gapi_dataWriterClaim(_this, &result);
    if (datawriter != NULL) {
        if (_Entity(datawriter)->enabled ) {
            result = _DataWriter_get_offered_deadline_missed_status (
                          datawriter,
                          TRUE,
                          status);
        } else {
            result=GAPI_RETCODE_NOT_ENABLED;
        }
    }

    _EntityRelease(datawriter);

    return result;
}

static v_result
copy_IncompatibleQosStatus(
    c_voidp info,
    c_voidp arg)
{
    unsigned long i;
    unsigned long len;
    v_result result = V_RESULT_PRECONDITION_NOT_MET;
    struct v_incompatibleQosInfo *from;
    gapi_offeredIncompatibleQosStatus *to;

    from = (struct v_incompatibleQosInfo *)info;
    to = (gapi_offeredIncompatibleQosStatus *)arg;

    to->total_count = from->totalCount;
    to->total_count_change = from->totalChanged;
    to->last_policy_id = from->lastPolicyId;

    len = c_arraySize(from->policyCount);
    if ( to->policies._buffer && (len <= to->policies._maximum) ) {
        to->policies._length = len;
        for ( i = 0; i < len; i++ ) {
            to->policies._buffer[i].policy_id = i;
            to->policies._buffer[i].count = ((c_long *)from->policyCount)[i];
        }
        result = V_RESULT_OK;
    }

    return result;
}

gapi_returnCode_t
_DataWriter_get_offered_incompatible_qos_status (
    _DataWriter _this,
    c_bool reset,
    gapi_offeredIncompatibleQosStatus *status)
{
    u_result uResult;
    uResult = u_writerGetIncompatibleQosStatus(
                  U_WRITER_GET(_this),
                  reset,
                  copy_IncompatibleQosStatus,
                  status);
    return kernelResultToApiResult(uResult);
}

gapi_returnCode_t
gapi_dataWriter_get_offered_incompatible_qos_status (
    gapi_dataWriter _this,
    gapi_offeredIncompatibleQosStatus *status)
{
    gapi_returnCode_t result;
    _DataWriter datawriter;

    datawriter = gapi_dataWriterClaim(_this, &result);
    if (datawriter != NULL) {
        if (_Entity(datawriter)->enabled ) {
            result = _DataWriter_get_offered_incompatible_qos_status (
                          datawriter,
                          TRUE,
                          status);
        } else {
            result=GAPI_RETCODE_NOT_ENABLED;
        }
    }

    _EntityRelease(datawriter);

    return result;
}

static v_result
copy_publication_matched_status(
    c_voidp info,
    c_voidp arg)
{
    struct v_topicMatchInfo *from;
    gapi_publicationMatchedStatus *to;

    from = (struct v_topicMatchInfo *)info;
    to = (gapi_publicationMatchedStatus *)arg;

    to->total_count = from->totalCount;
    to->total_count_change = from->totalChanged;
    to->current_count = from->totalCount;
    to->current_count_change = from->totalChanged;
    to->last_subscription_handle = u_instanceHandleFromGID(from->instanceHandle);
    return V_RESULT_OK;

}

gapi_returnCode_t
_DataWriter_get_publication_matched_status (
    _DataWriter _this,
    c_bool reset,
    gapi_publicationMatchedStatus *status)
{
    u_result uResult;
    uResult = u_writerGetPublicationMatchStatus(
                  U_WRITER_GET(_this),
                  reset,
                  copy_publication_matched_status,
                  status);
    return kernelResultToApiResult(uResult);
}

gapi_returnCode_t
gapi_dataWriter_get_publication_matched_status (
    gapi_dataWriter _this,
    gapi_publicationMatchedStatus *status)
{
    gapi_returnCode_t result;
    _DataWriter datawriter;

    datawriter = gapi_dataWriterClaim(_this, &result);
    if (datawriter != NULL) {
        if (_Entity(datawriter)->enabled ) {
            result = _DataWriter_get_publication_matched_status (
                          datawriter,
                          TRUE,
                          status);
        } else {
            result=GAPI_RETCODE_NOT_ENABLED;
        }
    }

    _EntityRelease(datawriter);

    return result;
}


/*     ReturnCode_t
 *     assert_liveliness();
 */
gapi_returnCode_t
gapi_dataWriter_assert_liveliness (
    gapi_dataWriter _this)
{
    gapi_returnCode_t result;
    u_result uResult;
    _DataWriter datawriter;

    datawriter = gapi_dataWriterClaim(_this, &result);

    if (datawriter != NULL) {
        if (_Entity(datawriter)->enabled ) {
            uResult = u_writerAssertLiveliness(U_WRITER_GET(datawriter));
            result = kernelResultToApiResult(uResult);
        } else {
            result=GAPI_RETCODE_NOT_ENABLED;
        }
    }

    _EntityRelease(datawriter);

    return result;
}

/*     ReturnCode_t
 *     get_matched_subscriptions(
 *         inout InstanceHandleSeq subscription_handles);
 */
gapi_returnCode_t
gapi_dataWriter_get_matched_subscriptions (
    gapi_dataWriter _this,
    gapi_instanceHandleSeq *subscription_handles)
{
    return GAPI_RETCODE_UNSUPPORTED;
}

/*     ReturnCode_t
 *     get_matched_subscription_data(
 *         inout SubscriptionBuiltinTopicData subscription_data,
 *         in InstanceHandle_t subscription_handle);
 */
gapi_returnCode_t
gapi_dataWriter_get_matched_subscription_data (
    gapi_dataWriter _this,
    gapi_subscriptionBuiltinTopicData *subscription_data,
    const gapi_instanceHandle_t subscription_handle)
{
    return GAPI_RETCODE_UNSUPPORTED;
}

gapi_instanceHandle_t
_DataWriterRegisterInstance (
    _DataWriter _this,
    const void *instanceData,
    c_time timestamp)
{
    u_instanceHandle handle;
    u_result uResult;
    writerInfo wData;
    writerInfo *pData = NULL;
    gapi_instanceHandle_t result = GAPI_HANDLE_NIL;

    if ( instanceData ) {
        wData.writer = _this;
        wData.data = (void *)instanceData;
        pData = &wData;
    }

    uResult = u_writerRegisterInstance(U_WRITER_GET(_this),
                                       (void *)pData,
                                       timestamp,
                                       &handle);

    if(uResult == U_RESULT_OK){
        result = (gapi_instanceHandle_t)handle;
    }
    return result;
}

gapi_returnCode_t
_DataWriterUnregisterInstance (
    _DataWriter _this,
    const void *instanceData,
    const gapi_instanceHandle_t handle,
    c_time timestamp)
{
    gapi_returnCode_t result = GAPI_RETCODE_OK;
    u_writer w;
    u_result uResult;
    writerInfo wData;
    writerInfo *pData = NULL;

    w = U_WRITER_GET(_this);
    if ( instanceData ) {
        wData.writer = _this;
        wData.data = (void *)instanceData;
        pData = &wData;
    }
    uResult = u_writerUnregisterInstance(w,
                                         pData,
                                         timestamp,
                                         (u_instanceHandle)handle);
    result = kernelResultToApiResult(uResult);
    return result;
}



gapi_returnCode_t
_DataWriterGetKeyValue (
    _DataWriter  _this,
    void        *instance,
    const gapi_instanceHandle_t handle)
{
    gapi_returnCode_t result;
    u_writer w;
    u_result          uResult;

    PREPEND_COPYOUTCACHE(_this->copy_cache,instance, NULL);

    w = U_WRITER_GET(_this);
    uResult = u_writerCopyKeysFromInstanceHandle(w,
                                       (u_instanceHandle)handle,
                                       (u_writerAction)_this->copy_out,
                                       instance);
    result = kernelResultToApiResult(uResult);

    REMOVE_COPYOUTCACHE(_this->copy_cache,instance);

    return result;
}

static void
onOfferedDeadlineMissed (
    _DataWriter _this)
{
    gapi_offeredDeadlineMissedStatus info;
    gapi_listener_OfferedDeadlineMissedListener callback;
    gapi_returnCode_t result;
    gapi_object target;
    gapi_object source;
    _Status status;
    _Entity entity;
    c_voidp listenerData;

    if ( _this ) {
        result = _DataWriter_get_offered_deadline_missed_status (
                     _this, TRUE, &info);

        /* Only allow the callback if there is a change since the last
         * callback, i.e if total_count_change is non zero
         */

        if (result == GAPI_RETCODE_OK && info.total_count_change != 0) {
            status = _Entity(_this)->status;
            source = _EntityHandle(_this);
            target = _StatusFindTarget(status,
                                       GAPI_OFFERED_DEADLINE_MISSED_STATUS);
            if (target) {
                if ( target != source ) {
                    entity = gapi_entityClaim(target, NULL);
                    status = entity->status;
                } else {
                    entity = NULL;
                }

                callback = status->callbackInfo.on_offered_deadline_missed;
                listenerData = status->callbackInfo.listenerData;

                _EntitySetBusy(_this);
                _EntityRelease(_this);

                if (entity) {
                    _EntitySetBusy(entity);
                    _EntityRelease(entity);
                    callback(listenerData, source, &info);
                    gapi_objectClearBusy(target);
                } else {
                    callback(listenerData, source, &info);
                }

                gapi_objectClearBusy(source);
                gapi_entityClaim(source, NULL);
            }
        }
    }
}

static void
onOfferedIncompatibleQos (
    _DataWriter _this)
{
    gapi_offeredIncompatibleQosStatus info;
    gapi_qosPolicyCount policyCount[MAX_POLICY_COUNT_ID];
    gapi_listener_OfferedIncompatibleQosListener callback;
    gapi_returnCode_t result;
    gapi_object target;
    gapi_object source;
    _Entity entity;
    _Status status;
    c_voidp listenerData;

    if ( _this ) {

        info.policies._maximum = MAX_POLICY_COUNT_ID;
        info.policies._length  = 0;
        info.policies._buffer  = policyCount;

        result = _DataWriter_get_offered_incompatible_qos_status (
                     _this, TRUE, &info);

        /* Only allow the callback if there is a change since the last
         * callback, i.e if total_count_change is non zero
         */

        if (result == GAPI_RETCODE_OK && info.total_count_change != 0) {
            status = _Entity(_this)->status;
            source = _EntityHandle(_this);
            target = _StatusFindTarget(status,
                                       GAPI_OFFERED_INCOMPATIBLE_QOS_STATUS);
            if (target) {
                if ( target != source ) {
                    entity = gapi_entityClaim(target, NULL);
                    status = entity->status;
                } else {
                    entity = NULL;
                }

                callback = status->callbackInfo.on_offered_incompatible_qos;
                listenerData = status->callbackInfo.listenerData;

                _EntitySetBusy(_this);
                _EntityRelease(_this);

                if (entity) {
                    _EntitySetBusy(entity);
                    _EntityRelease(entity);
                    callback(listenerData, source, &info);
                    gapi_objectClearBusy(target);
                } else {
                    callback(listenerData, source, &info);
                }

                gapi_objectClearBusy(source);
                gapi_entityClaim(source, NULL);
            }
        }
    }
}

static void
onLivelinessLost (
    _DataWriter _this)
{
    gapi_livelinessLostStatus info;
    gapi_listener_LivelinessLostListener callback;
    gapi_returnCode_t result;
    gapi_object target;
    gapi_object source;
    _Entity entity;
    _Status status;
    c_voidp listenerData;

    if ( _this ) {
        result = _DataWriter_get_liveliness_lost_status (
                     _this, TRUE, &info);

        /* Only allow the callback if there is a change since the last
         * callback, i.e if total_count_change is non zero
         */

        if (result == GAPI_RETCODE_OK && info.total_count_change != 0) {
            status = _Entity(_this)->status;
            source = _EntityHandle(_this);
            target = _StatusFindTarget(status,
                                       GAPI_LIVELINESS_LOST_STATUS);
            if (target) {
                if ( target != source ) {
                    entity = gapi_entityClaim(target, NULL);
                    status = entity->status;
                } else {
                    entity = NULL;
                }

                callback = status->callbackInfo.on_liveliness_lost;
                listenerData = status->callbackInfo.listenerData;

                _EntitySetBusy(_this);
                _EntityRelease(_this);

                if (entity) {
                    _EntitySetBusy(entity);
                    _EntityRelease(entity);
                    callback(listenerData, source, &info);
                    gapi_objectClearBusy(target);
                } else {
                    callback(listenerData, source, &info);
                }

                gapi_objectClearBusy(source);
                gapi_entityClaim(source, NULL);
            }
        }
    }
}

static void
onPublicationMatch (
    _DataWriter _this)
{
    _Status status;
    gapi_publicationMatchedStatus info;
    gapi_listener_PublicationMatchedListener callback;
    gapi_returnCode_t result;
    gapi_object target;
    gapi_object source;
    _Entity entity;
    c_voidp listenerData;

    if ( _this ) {
        result = _DataWriter_get_publication_matched_status (
                     _this, TRUE, &info);

        /* Only allow the callback if there is a change since the last
         * callback, i.e if total_count_change is non zero
         */

        if (result == GAPI_RETCODE_OK && info.total_count_change != 0) {
            status = _Entity(_this)->status;
            source = _EntityHandle(_this);
            target = _StatusFindTarget(status,
                                       GAPI_PUBLICATION_MATCH_STATUS);
            if (target) {
                if ( target != source ) {
                    entity = gapi_entityClaim(target, NULL);
                    status = entity->status;
                } else {
                    entity = NULL;
                }

                callback = status->callbackInfo.on_publication_match;
                listenerData = status->callbackInfo.listenerData;

                _EntitySetBusy(_this);
                _EntityRelease(_this);

                if (entity) {
                    _EntitySetBusy(entity);
                    _EntityRelease(entity);
                    callback(listenerData, source, &info);
                    gapi_objectClearBusy(target);
                } else {
                    callback(listenerData, source, &info);
                }

                gapi_objectClearBusy(source);
                gapi_entityClaim(source, NULL);
            }
        }
    }
}


void
_DataWriterNotifyListener(
    _DataWriter _this,
    gapi_statusMask triggerMask)
{
    while ( _this && (triggerMask != GAPI_STATUS_KIND_NULL) ) {
        if ( triggerMask & GAPI_LIVELINESS_LOST_STATUS ) {
            onLivelinessLost(_this);
            triggerMask &= ~GAPI_LIVELINESS_LOST_STATUS;
        }
        if ( triggerMask & GAPI_OFFERED_DEADLINE_MISSED_STATUS ) {
            onOfferedDeadlineMissed(_this);
            triggerMask &= ~GAPI_OFFERED_DEADLINE_MISSED_STATUS;
        }
        if ( triggerMask & GAPI_OFFERED_INCOMPATIBLE_QOS_STATUS ) {
            onOfferedIncompatibleQos(_this);
            triggerMask &= ~GAPI_OFFERED_INCOMPATIBLE_QOS_STATUS;
        }
        if ( triggerMask & GAPI_PUBLICATION_MATCH_STATUS ) {
            onPublicationMatch(_this);
            triggerMask &= ~GAPI_PUBLICATION_MATCH_STATUS;
        }
    }
}

