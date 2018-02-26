#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/time.h>

#include <scorep/SCOREP_SubstratePlugins.h>

#define PREFIX "SCOREP_SUBSTRATE_PRINT_METRICS"

/* This struct will exist for every metric registered via SCOREP_SUBSTRATE_PRINT_METRICS */
typedef struct
{
  /* The name as read from SCOREP_SUBSTRATE_PRINT_METRICS */
  char * name;
  /* will be initially invalid, and set when the metric is defined, used for information about metric
   * If this is still SCOREP_INVALID_METRIC, the process hasn't defined the metric. */
  SCOREP_MetricHandle metric_handle;

  /* will be initially invalid, and set when the metric is defined, describes the sampling set (a set which holds multiple metrics */
  SCOREP_SamplingSetHandle sset_handle;

  /* will be initially invalid, and set when the metric is defined */
  uint64_t metric_index_in_sset;

  /* the mode of the metric is buffered here so we don't have to call the callback each time */
  SCOREP_MetricMode mode;
  /* the value type of the metric is buffered here so we don't have to call the callback each time */
  SCOREP_MetricValueType value_type;

  /* the current value, will be increased with every new value delivered from score-p */
  union
  {
    uint64_t ui;
    int64_t i;
    double d;
  } value;

  /* the first value ever read */
  union
  {
    uint64_t ui;
    int64_t i;
    double d;
  } first_value;

  /* the total number of values. when this == 0 then no values were written */
  uint64_t nr_values;
} metric_info_t;

/* Callback functions from Score-P */
static const SCOREP_SubstratePluginCallbacks* callbacks;

/* Location of the main thread */
static const struct SCOREP_Location* process_location;

/* metric information */
static metric_info_t * metrics;
static int nr_metrics;

/* sets the Score-P callbacks */
static void
print_metrics_set_callbacks (
    const SCOREP_SubstratePluginCallbacks* callbacks_in, size_t size)
{
  assert(size >= sizeof(SCOREP_SubstratePluginCallbacks));
  callbacks = callbacks_in;
}

/* Show info. Could be improved. */
static void
print_usage ()
{
  printf ("This plugin does not work if you do not set "PREFIX"\n");
}

/* Called when MPP is initialized (e.g., when MPI_Init() is called) or after init when no MPP is used */
static void
print_metrics_init_mpp (void)
{
  /* Get the metrics from env. variable */
  char* env_metrics = getenv (PREFIX);
  if (env_metrics == NULL)
    {
      print_usage ();
      return;
    }
  char* env_metrics_separator = getenv (PREFIX"_SEP");
  if (env_metrics_separator == NULL)
    env_metrics_separator = ",";

  /* Count number of separator characters in list of metric names */
  char * position = env_metrics;
  // at least one element, the env is set, right
  size_t list_alloc = 1;
  while (*position)
    {
      if (strchr (env_metrics_separator, *position))
	{
	  list_alloc++;
	}
      position++;
    }

  /* Allocate memory for array of metric names */
  metrics = calloc (list_alloc, sizeof(metric_info_t));
  assert(metrics != NULL);

  /* Parse list of metric names */
  char* token = strtok (env_metrics, env_metrics_separator);
  while (token)
    {
      assert(nr_metrics < list_alloc);

      metrics[nr_metrics].name = strdup (token);
      metrics[nr_metrics].metric_handle = SCOREP_INVALID_METRIC;
      metrics[nr_metrics].sset_handle = SCOREP_INVALID_SAMPLING_SET;

      token = strtok ( NULL, env_metrics_separator);
      nr_metrics++;
    }
}

/* called whenever there's a new definition */
static void
print_metrics_new_definition_handle (SCOREP_AnyHandle handle,
				     SCOREP_HandleType type)
{
  switch (type)
    {
    /* we only care for sampling sets */
    case SCOREP_HANDLE_TYPE_SAMPLING_SET:
      {
	uint8_t nr_metric_handles =
	    callbacks->SCOREP_SamplingSetHandle_GetNumberOfMetrics (handle);
	const SCOREP_MetricHandle * mets =
	    callbacks->SCOREP_SamplingSetHandle_GetMetricHandles (handle);
	for (uint8_t met_nr = 0; met_nr < nr_metric_handles; met_nr++)
	  {
	    SCOREP_MetricHandle metric_handle = mets[met_nr];
	    const char * name = callbacks->SCOREP_MetricHandle_GetName (
		metric_handle);
	    assert(name != NULL);

	    size_t i;
	    for (i = 0; i < nr_metrics; i++)
	      {

		if (strcmp (name, metrics[i].name) == 0)
		  break;
	      }
	    /* if no metric is in the sampling set */
	    if (i == nr_metrics)
	      continue;

	    /* must be asynchronous */
	    assert(
		callbacks->SCOREP_SamplingSetHandle_GetMetricOccurrence (handle)
		    == SCOREP_METRIC_OCCURRENCE_ASYNCHRONOUS);

	    /* set metric definition */
	    metrics[i].metric_handle = metric_handle;
	    metrics[i].sset_handle = handle;
	    metrics[i].metric_index_in_sset = met_nr;
	    metrics[i].mode = callbacks->SCOREP_MetricHandle_GetMode (
		metric_handle);
	    printf ("%s mode: %d\n", metrics[i].name, metrics[i].mode);
	    metrics[i].value_type =
		callbacks->SCOREP_MetricHandle_GetValueType (metric_handle);
	  }
	break;
      }
    default: // ignore
      return;
    }
}

/* will be called when the metrics are written (either at an event or at the end of a run) */
static void
print_metrics_write_metric (struct SCOREP_Location* location,
			    uint64_t timestamp,
			    SCOREP_SamplingSetHandle samplingSet,
			    const uint64_t* metricValues)
{
  /* metric might be double */
  double* metricValuesD = (double *) metricValues;
  for (size_t i = 0; i < nr_metrics; i++)
    {
      if (samplingSet == metrics[i].sset_handle)
	{
	  // assume that its double
	  switch (metrics[i].mode)
	    {
	    case SCOREP_METRIC_MODE_ACCUMULATED_START:
	    case SCOREP_METRIC_MODE_ACCUMULATED_POINT:
	    case SCOREP_METRIC_MODE_ACCUMULATED_LAST:
	    case SCOREP_METRIC_MODE_ACCUMULATED_NEXT:
	      // todo this should take time into account
	      switch (metrics[i].value_type)
		{
		case SCOREP_METRIC_VALUE_DOUBLE:
		  metrics[i].value.d =
		      metricValuesD[metrics[i].metric_index_in_sset];
		  if (metrics[i].first_value.i == 0)
		    metrics[i].first_value.d = metrics[i].value.d;
		  break;
		default:
		  metrics[i].value.i =
		      metricValues[metrics[i].metric_index_in_sset];
		  if (metrics[i].first_value.i == 0)
		    metrics[i].first_value.i = metrics[i].value.i;
		  break;
		}
	      break;
	    case SCOREP_METRIC_MODE_ABSOLUTE_POINT:
	    case SCOREP_METRIC_MODE_ABSOLUTE_LAST:
	    case SCOREP_METRIC_MODE_ABSOLUTE_NEXT:
	      // todo scale with time for last and next
	      switch (metrics[i].value_type)
		{
		case SCOREP_METRIC_VALUE_DOUBLE:
		  metrics[i].value.d +=
		      metricValuesD[metrics[i].metric_index_in_sset];
		  break;
		default:
		  metrics[i].value.i +=
		      metricValues[metrics[i].metric_index_in_sset];
		  break;
		}
	      break;
	      // should not happen
	    default:
	      break;
	    }
	  metrics[i].nr_values++;
	}
    }
}

/* for measuring time between init and unify */
static struct timeval start, stop;

/* called after unify */
static void
print_metrics_write_data ()
{
  // collect info for async last
  for (size_t i = 0; i < nr_metrics; i++)
    {
      /* the sum of all averages from all processes (will result from reduce) */
      double sum = 0.0;
      /* the total number of recorded values (will result from reduce) */
      uint64_t total_nr_values = 0;
      /* did this process record any values? */
      uint64_t collected_data = (metrics[i].nr_values == 0) ? 0 : 1;
      /* number of participating processes (will result from reduce)*/
      uint64_t participating_processes = 0;
      double avg;
      double sum_avg;

      // unify the type and mode. some processes do not have the definition
      int64_t in = metrics[i].value_type;
      int64_t out;
      callbacks->SCOREP_Ipc_Allreduce (&in, &out, 1, SCOREP_IPC_INT64_T,
				       SCOREP_IPC_MAX);
      SCOREP_MetricValueType type = out;
      in = metrics[i].mode;
      callbacks->SCOREP_Ipc_Allreduce (&in, &out, 1, SCOREP_IPC_INT64_T,
				       SCOREP_IPC_MAX);
      SCOREP_MetricMode mode = out;

      /* now every process knows mode and type */
      /* compute average for all processes */
      if (metrics[i].nr_values > 0)
	{
	  /* data was recorded, compute avg. */
	  switch (mode)
	    {
	    case SCOREP_METRIC_MODE_ACCUMULATED_START:
	    case SCOREP_METRIC_MODE_ACCUMULATED_POINT:
	    case SCOREP_METRIC_MODE_ACCUMULATED_LAST:
	    case SCOREP_METRIC_MODE_ACCUMULATED_NEXT:
	      {
		double time = (double) stop.tv_sec - (double) start.tv_sec
		    + (double) stop.tv_usec / 1000000.0
		    - (double) start.tv_usec / 1000000.0;
		switch (type)
		  {
		  case SCOREP_METRIC_VALUE_DOUBLE:
		    avg = metrics[i].value.d / time;
		    break;
		  default:
		    avg = metrics[i].value.i / time;
		    break;
		  }
		break;
	      }
	    case SCOREP_METRIC_MODE_ABSOLUTE_POINT:
	    case SCOREP_METRIC_MODE_ABSOLUTE_LAST:
	    case SCOREP_METRIC_MODE_ABSOLUTE_NEXT:
	      {
		switch (type)
		  {
		  case SCOREP_METRIC_VALUE_DOUBLE:
		    avg = metrics[i].value.d / metrics[i].nr_values;
		    break;
		  default:
		    avg = (double) metrics[i].value.i
			/ (double) metrics[i].nr_values;
		    break;
		  }
		break;
	      }
	    }
	}
      else
	/* data was not recorded, avg == 0 */
	avg = 0.0;
      /* hand it to rank 0 */
      callbacks->SCOREP_Ipc_Reduce (&avg, &sum_avg, 1, SCOREP_IPC_DOUBLE,
				    SCOREP_IPC_SUM, 0);
      /* tell rank 0 how many processes were involved */
      callbacks->SCOREP_Ipc_Reduce (&collected_data, &participating_processes,
				    1, SCOREP_IPC_UINT64_T, SCOREP_IPC_SUM, 0);
      /* tell rank 0 how many values were recorded  */
      callbacks->SCOREP_Ipc_Reduce (&metrics[i].nr_values, &total_nr_values, 1,
				    SCOREP_IPC_UINT64_T, SCOREP_IPC_SUM, 0);

      /* now rank 0 prints the information */
      if (callbacks->SCOREP_Location_GetGlobalId (process_location) == 0)
	{
	  if (participating_processes == 0)
	    printf (
		"There are no recordings for metric %s. The metric has %sbeen defined\n",
		metrics[i].name,
		metrics[i].metric_handle == SCOREP_INVALID_METRIC ?
		    "not " : "");
	  else
	    {
	      double time = (double) stop.tv_sec - (double) start.tv_sec
		  + (double) stop.tv_usec / 1000000.0
		  - (double) start.tv_usec / 1000000.0;
	      printf (
		  "%s: average %f, time %f s, participating processes %" PRIu64 "\n",
		  metrics[i].name, sum_avg / ((double) participating_processes),
		  time, participating_processes);
	    }
	}
    }
}

/* free data structures */
static void
print_metrics_finalize ()
{
  for (size_t i = 0; i < nr_metrics; i++)
    {
      free (metrics[i].name);
    }
  free (metrics);
}

/*we only care for asynch metrics, so that's it */
static SCOREP_Substrates_Callback evts[SCOREP_SUBSTRATES_NUM_EVENTS];

static uint32_t
print_metrics_get_event_functions (SCOREP_Substrates_Mode mode,
				   SCOREP_Substrates_Callback** functions)
{
  /* TODO change this to SCOREP_EVENT_WRITE_ASYNC_METRIC_BEFORE_EVENT when merge */
  evts[SCOREP_EVENT_WRITE_METRIC_BEFORE_EVENT] =
      (SCOREP_Substrates_Callback) print_metrics_write_metric;

  *functions = evts;
  return SCOREP_SUBSTRATES_NUM_EVENTS;
}

/* when the first location is created, we store the process location id */
static void
print_metrics_create_location (const struct SCOREP_Location* location,
			       const struct SCOREP_Location* parentLocation)
{
  if (process_location == NULL)
    process_location = location;
}

/* take time at init and at unify */
static void
print_metrics_init (size_t ignored)
{
  gettimeofday (&start, NULL);
}

static void
print_metrics_unify ()
{
  gettimeofday (&stop, NULL);
}

/* define the plugin */
SCOREP_SUBSTRATE_PLUGIN_ENTRY (print_metrics)
{
  SCOREP_SubstratePluginInfo info;
  memset (&info, 0, sizeof(SCOREP_SubstratePluginInfo));
  info.plugin_version = SCOREP_SUBSTRATE_PLUGIN_VERSION;
  info.assign_id = print_metrics_init;
  info.init_mpp = print_metrics_init_mpp;
  info.finalize = print_metrics_finalize;
  info.write_data = print_metrics_write_data;
  info.pre_unify = print_metrics_unify;
  info.set_callbacks = print_metrics_set_callbacks;
  info.new_definition_handle = print_metrics_new_definition_handle;
  info.get_event_functions = print_metrics_get_event_functions;
  info.create_location = print_metrics_create_location;
  return info;
}
