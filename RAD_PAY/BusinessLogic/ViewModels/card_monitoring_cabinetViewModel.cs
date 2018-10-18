using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_monitoring_cabinetViewModel
    {
        public long id { get; set; }
        public long card_id { get; set; }
        public int? monitoring_flag { get; set; }
        public DateTime? add_ts { get; set; }
        public DateTime? start_date { get; set; }
        public DateTime? end_date { get; set; }
        public long? purchase_id { get; set; }
        public int? status { get; set; }
        public DateTime? off_ts { get; set; }
        public long? periodic_id { get; set; }
        public long? uid { get; set; }
    }
}