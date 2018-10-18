using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class periodic_billViewModel
    {
        public long id { get; set; }
        public long? uid { get; set; }
        public int? merchant_id { get; set; }
        public int? field_id { get; set; }
        public long? amount { get; set; }
        public string name { get; set; }
        public string value { get; set; }
        public string periodic_ts { get; set; }
        public string prefix { get; set; }
        public DateTime add_ts { get; set; }
        public int? status { get; set; }
        public DateTime? last_bill_ts { get; set; }
        public DateTime? last_notify_ts { get; set; }
        public long card_id { get; set; }
    }
}