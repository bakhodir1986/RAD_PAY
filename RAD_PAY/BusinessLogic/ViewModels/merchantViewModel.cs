using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class merchantViewModel
    {
        public int id { get; set; }
        public string name { get; set; }
        public string url { get; set; }
        public int? group_id { get; set; }
        public int? status { get; set; }
        public string inn { get; set; }
        public string contract { get; set; }
        public DateTime? contract_date { get; set; }
        public string mfo { get; set; }
        public string ch_account { get; set; }
        public string merchant_id { get; set; }
        public string terminal_id { get; set; }
        public int? port { get; set; }
        public long? min_amount { get; set; }
        public long? max_amount { get; set; }
        public int external { get; set; }
        public string ext_service_id { get; set; }
        public int? bank_id { get; set; }
        public int rate { get; set; }
        public long rate_money { get; set; }
        public int? position { get; set; }
        public int api_id { get; set; }
        public long icon_id { get; set; }
    }
}