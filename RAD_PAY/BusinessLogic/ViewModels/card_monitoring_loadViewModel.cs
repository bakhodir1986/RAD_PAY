using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class card_monitoring_loadViewModel
    {
        public long id { get; set; }
        public long? card_id { get; set; }
        public DateTime? from_date { get; set; }
        public DateTime? to_date { get; set; }
        public DateTime? ts { get; set; }
        public int? status { get; set; }
    }
}