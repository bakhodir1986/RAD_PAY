using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;

namespace RAD_PAY.BusinessLogic.ViewModels
{
    public class transactionViewModel
    {
        public long id { get; set; }
        public long? uid { get; set; }
        public string srccard { get; set; }
        public string dstcard { get; set; }
        public string srcphone { get; set; }
        public string dstphone { get; set; }
        public long? amount { get; set; }
        public DateTime? ts { get; set; }
        public int? status { get; set; }
        public long? dst_uid { get; set; }
        public long? commision { get; set; }
        public string ref_num { get; set; }
        public long? bearn { get; set; }
        public string status_text { get; set; }
    }
}