using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class faultViewModel
    {
        public long id { get; set; }
        public int? type { get; set; }
        public DateTime? ts { get; set; }
        public int? status { get; set; }
        public string description { get; set; }
        public DateTime? ts_notify { get; set; }
    }
}