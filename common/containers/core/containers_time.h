#ifndef VC_CONTAINERS_TIME_H
#define VC_CONTAINERS_TIME_H

/** \file
 * Utility functions to help with timestamping of elementary stream frames
 */

typedef struct VC_CONTAINER_TIME_T
{
   uint32_t samplerate_num;
   uint32_t samplerate_den;
   uint32_t time_base;

   uint32_t remainder;

   int64_t time;

} VC_CONTAINER_TIME_T;

/*****************************************************************************/
STATIC_INLINE void vc_container_time_init( VC_CONTAINER_TIME_T *time, uint32_t time_base )
{
   time->samplerate_num = 0;
   time->samplerate_den = 0;
   time->remainder = 0;
   time->time_base = time_base;
   time->time = VC_CONTAINER_TIME_UNKNOWN;
}

/*****************************************************************************/
STATIC_INLINE int64_t vc_container_time_get( VC_CONTAINER_TIME_T *time )
{
   if (time->time == VC_CONTAINER_TIME_UNKNOWN || !time->samplerate_num || !time->samplerate_den)
      return VC_CONTAINER_TIME_UNKNOWN;
   return time->time + time->remainder * (int64_t)time->time_base * time->samplerate_den / time->samplerate_num;
}

/*****************************************************************************/
STATIC_INLINE void vc_container_time_set_samplerate( VC_CONTAINER_TIME_T *time, uint32_t samplerate_num, uint32_t samplerate_den )
{
   if(time->samplerate_num == samplerate_num &&
      time->samplerate_den == samplerate_den)
      return;

   /* We're changing samplerate, we need to reset our remainder */
   if(time->remainder)
      time->time = vc_container_time_get( time );
   time->remainder = 0;
   time->samplerate_num = samplerate_num;
   time->samplerate_den = samplerate_den;
}

/*****************************************************************************/
STATIC_INLINE void vc_container_time_set( VC_CONTAINER_TIME_T *time, int64_t new_time )
{
   if (new_time == VC_CONTAINER_TIME_UNKNOWN)
      return;
   time->remainder = 0;
   time->time = new_time;
}

/*****************************************************************************/
STATIC_INLINE int64_t vc_container_time_add( VC_CONTAINER_TIME_T *time, uint32_t samples )
{
   uint32_t increment;

   if (time->time == VC_CONTAINER_TIME_UNKNOWN || !time->samplerate_num || !time->samplerate_den)
      return VC_CONTAINER_TIME_UNKNOWN;

   samples += time->remainder;
   increment = samples * time->samplerate_den / time->samplerate_num;
   time->time += increment * time->time_base;
   time->remainder = samples - increment * time->samplerate_num / time->samplerate_den;
   return vc_container_time_get(time);
}

#endif /* VC_CONTAINERS_TIME_H */
